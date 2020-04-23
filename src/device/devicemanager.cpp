/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <memory>
#include <functional>

#include <QtGlobal>
#include <QApplication>
#include <QObject>
#include <QMetaObject>
#include <QThread>
#include <QAbstractItemModel>
#include <QDir>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QUrl>
#include <QPixmap>
#include <QPainter>
#include <QMessageBox>
#include <QPushButton>
#include <QtDebug>

#include "devicemanager.h"

#include "core/application.h"
#include "core/concurrentrun.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/musicstorage.h"
#include "core/taskmanager.h"
#include "core/utilities.h"
#include "core/simpletreemodel.h"
#include "filesystemdevice.h"
#include "connecteddevice.h"
#include "devicelister.h"
#include "devicedatabasebackend.h"
#include "devicestatefiltermodel.h"
#include "deviceinfo.h"

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
#  include "cddalister.h"
#  include "cddadevice.h"
#endif
#ifdef HAVE_DBUS
#  ifdef HAVE_UDISKS2
#    include "udisks2lister.h"
#  endif
#endif
#ifdef HAVE_LIBMTP
#  include "mtpdevice.h"
#endif
#ifdef HAVE_IMOBILEDEVICE
#  include "afcdevice.h"
#  include "ilister.h"
#endif
#if defined(Q_OS_MACOS) and defined(HAVE_LIBMTP)
#  include "macosdevicelister.h"
#endif
#ifdef HAVE_LIBGPOD
#  include "gpoddevice.h"
#endif
#ifdef HAVE_GIO
#  include "giolister.h" // Needs to be last because of #undef signals.
#endif

const int DeviceManager::kDeviceIconSize = 32;
const int DeviceManager::kDeviceIconOverlaySize = 16;

DeviceManager::DeviceManager(Application *app, QObject *parent)
    : SimpleTreeModel<DeviceInfo>(new DeviceInfo(this), parent),
    app_(app),
    not_connected_overlay_(IconLoader::Load("edit-delete")) {

  thread_pool_.setMaxThreadCount(1);
  connect(app_->task_manager(), SIGNAL(TasksChanged()), SLOT(TasksChanged()));

  // Create the backend in the database thread
  backend_ = new DeviceDatabaseBackend;
  backend_->moveToThread(app_->database()->thread());
  backend_->Init(app_->database());

  connect(this, SIGNAL(DeviceCreatedFromDB(DeviceInfo*)), SLOT(AddDeviceFromDB(DeviceInfo*)));

  // This reads from the database and contents on the database mutex, which can be very slow on startup.
  ConcurrentRun::Run<void>(&thread_pool_, std::bind(&DeviceManager::LoadAllDevices, this));

  // This proxy model only shows connected devices
  connected_devices_model_ = new DeviceStateFilterModel(this);
  connected_devices_model_->setSourceModel(this);

// CD devices are detected via the DiskArbitration framework instead on MacOs.
#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER) && !defined(Q_OS_MACOS)
  AddLister(new CddaLister);
#endif
#if defined(HAVE_DBUS) && defined(HAVE_UDISKS2)
  AddLister(new Udisks2Lister);
#endif
#ifdef HAVE_GIO
  AddLister(new GioLister);
#endif
#if defined(Q_OS_MACOS) and defined(HAVE_LIBMTP)
  AddLister(new MacOsDeviceLister);
#endif
#ifdef HAVE_IMOBILEDEVICE
  AddLister(new iLister);
#endif

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  AddDeviceClass<CddaDevice>();
#endif

  AddDeviceClass<FilesystemDevice>();

#ifdef HAVE_LIBGPOD
  AddDeviceClass<GPodDevice>();
#endif

#ifdef HAVE_LIBMTP
  AddDeviceClass<MtpDevice>();
#endif

#ifdef HAVE_IMOBILEDEVICE
  AddDeviceClass<AfcDevice>();
#endif

}

DeviceManager::~DeviceManager() {

  for (DeviceLister *lister : listers_) {
    lister->ShutDown();
    delete lister;
  }
  listers_.clear();

  delete root_;
  root_ = nullptr;

  backend_->deleteLater();
  backend_ = nullptr;

}

void DeviceManager::Exit() {
  CloseDevices();
}

void DeviceManager::CloseDevices() {

  for (DeviceInfo *info : devices_) {
    if (!info->device_) continue;
    if (wait_for_exit_.contains(info->device_.get())) continue;
    wait_for_exit_ << info->device_.get();
    connect(info->device_.get(), SIGNAL(destroyed()), SLOT(DeviceDestroyed()));
    info->device_->Close();
  }
  if (wait_for_exit_.isEmpty()) CloseListers();

}

void DeviceManager::CloseListers() {

  for (DeviceLister *lister : listers_) {
    if (wait_for_exit_.contains(lister)) continue;
    wait_for_exit_ << lister;
    connect(lister, SIGNAL(ExitFinished()), this, SLOT(ListerClosed()));
    lister->ExitAsync();
  }
  if (wait_for_exit_.isEmpty()) CloseBackend();

}

void DeviceManager::CloseBackend() {

  if (!backend_ || wait_for_exit_.contains(backend_)) return;
  wait_for_exit_ << backend_;
  connect(backend_, SIGNAL(ExitFinished()), this, SLOT(BackendClosed()));
  backend_->ExitAsync();

}

void DeviceManager::BackendClosed() {

  QObject *obj = static_cast<QObject*>(sender());
  disconnect(obj, 0, this, 0);
  qLog(Debug) << obj << "successfully closed.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) emit ExitFinished();

}

void DeviceManager::ListerClosed() {

  DeviceLister *lister = static_cast<DeviceLister*>(sender());
  if (!lister) return;

  disconnect(lister, 0, this, 0);
  qLog(Debug) << lister << "successfully closed.";
  wait_for_exit_.removeAll(lister);

  if (wait_for_exit_.isEmpty()) CloseBackend();

}

void DeviceManager::DeviceDestroyed() {

  ConnectedDevice *device = static_cast<ConnectedDevice*>(sender());
  if (!wait_for_exit_.contains(device) || !backend_) return;

  wait_for_exit_.removeAll(device);
  if (wait_for_exit_.isEmpty()) CloseListers();

}
void DeviceManager::LoadAllDevices() {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  DeviceDatabaseBackend::DeviceList devices = backend_->GetAllDevices();
  for (const DeviceDatabaseBackend::Device &device : devices) {
    DeviceInfo *info = new DeviceInfo(DeviceInfo::Type_Device, root_);
    info->InitFromDb(device);
    emit DeviceCreatedFromDB(info);
  }

  // This is done in a concurrent thread so close the unique DB connection.
  backend_->Close();

}

void DeviceManager::AddDeviceFromDB(DeviceInfo *info) {

  QStringList icon_names = info->icon_name_.split(',');
  QVariantList icons;
  for (const QString &icon_name : icon_names) {
    icons << icon_name;
  }
  info->LoadIcon(icons, info->friendly_name_);

  DeviceInfo *existing = FindEquivalentDevice(info);
  if (existing) {
    qLog(Info) << "Found existing device: " << info->friendly_name_;
    existing->icon_name_ = info->icon_name_;
    existing->icon_ = info->icon_;
    QModelIndex idx = ItemToIndex(existing);
    if (idx.isValid()) emit dataChanged(idx, idx);
    delete info;
  }
  else {
    qLog(Info) << "Device added from database: " << info->friendly_name_;
    beginInsertRows(ItemToIndex(root_), devices_.count(), devices_.count());
    devices_ << info;
    endInsertRows();
  }

}

QVariant DeviceManager::data(const QModelIndex &idx, int role) const {

  if (!idx.isValid() || idx.column() != 0) return QVariant();

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return QVariant();

  switch (role) {
    case Qt::DisplayRole: {
      QString text;
      if (!info->friendly_name_.isEmpty())
        text = info->friendly_name_;
      else if (info->BestBackend())
        text = info->BestBackend()->unique_id_;

      if (info->size_)
        text = text + QString(" (%1)").arg(Utilities::PrettySize(info->size_));
      if (info->device_.get()) info->device_->Refresh();
      return text;
    }

    case Qt::DecorationRole: {
      QPixmap pixmap = info->icon_.pixmap(kDeviceIconSize);

      if (info->backends_.isEmpty() || !info->BestBackend() || !info->BestBackend()->lister_) {
        // Disconnected but remembered
        QPainter p(&pixmap);
        p.drawPixmap(kDeviceIconSize - kDeviceIconOverlaySize, kDeviceIconSize - kDeviceIconOverlaySize, not_connected_overlay_.pixmap(kDeviceIconOverlaySize));
      }

      return pixmap;
    }

    case Role_FriendlyName:
      return info->friendly_name_;

    case Role_UniqueId:
      if (!info->BestBackend()) return QString();
      return info->BestBackend()->unique_id_;

    case Role_IconName:
      return info->icon_name_;

    case Role_Capacity:
    case MusicStorage::Role_Capacity:
      return info->size_;

    case Role_FreeSpace:
    case MusicStorage::Role_FreeSpace:
      return ((info->BestBackend() && info->BestBackend()->lister_) ? info->BestBackend()->lister_->DeviceFreeSpace(info->BestBackend()->unique_id_) : QVariant());

    case Role_State:
      if (info->device_) return State_Connected;
      if (info->BestBackend() && info->BestBackend()->lister_) {
        if (info->BestBackend()->lister_->DeviceNeedsMount(info->BestBackend()->unique_id_)) return State_NotMounted;
        return State_NotConnected;
      }
      return State_Remembered;

    case Role_UpdatingPercentage:
      if (info->task_percentage_ == -1) return QVariant();
      return info->task_percentage_;

    case MusicStorage::Role_Storage:
      if (!info->device_ && info->database_id_ != -1)
        const_cast<DeviceManager*>(this)->Connect(info);
      if (!info->device_) return QVariant();
      return QVariant::fromValue<std::shared_ptr<MusicStorage>>(info->device_);

    case MusicStorage::Role_StorageForceConnect:
      if (!info->BestBackend()) return QVariant();
      if (!info->device_) {
        if (info->database_id_ == -1 && !info->BestBackend()->lister_->DeviceNeedsMount(info->BestBackend()->unique_id_)) {
          if (info->BestBackend()->lister_->AskForScan(info->BestBackend()->unique_id_)) {
            std::unique_ptr<QMessageBox> dialog(new QMessageBox(QMessageBox::Information, tr("Connect device"), tr("This is the first time you have connected this device.  Strawberry will now scan the device to find music files - this may take some time."), QMessageBox::Cancel));
            QPushButton *connect = dialog->addButton(tr("Connect device"), QMessageBox::AcceptRole);
            dialog->exec();
            if (dialog->clickedButton() != connect) return QVariant();
          }
        }
        const_cast<DeviceManager*>(this)->Connect(info);
      }
      if (!info->device_) return QVariant();
      return QVariant::fromValue<std::shared_ptr<MusicStorage>>(info->device_);

    case Role_MountPath: {
      if (!info->device_) return QVariant();

      QString ret = info->device_->url().path();
#ifdef Q_OS_WIN32
      if (ret.startsWith('/')) ret.remove(0, 1);
#endif
      return QDir::toNativeSeparators(ret);
    }

    case Role_TranscodeMode:
      return info->transcode_mode_;

    case Role_TranscodeFormat:
      return info->transcode_format_;

    case Role_SongCount:
      if (!info->device_) return QVariant();
      return info->device_->song_count();

    case Role_CopyMusic:
      if (info->BestBackend() && info->BestBackend()->lister_) return info->BestBackend()->lister_->CopyMusic();
      else return false;

    default:
      return QVariant();
  }

}

void DeviceManager::AddLister(DeviceLister *lister) {

  listers_ << lister;
  connect(lister, SIGNAL(DeviceAdded(QString)), SLOT(PhysicalDeviceAdded(QString)));
  connect(lister, SIGNAL(DeviceRemoved(QString)), SLOT(PhysicalDeviceRemoved(QString)));
  connect(lister, SIGNAL(DeviceChanged(QString)), SLOT(PhysicalDeviceChanged(QString)));

  lister->Start();

}

DeviceInfo *DeviceManager::FindDeviceById(const QString &id) const {

  for (int i = 0; i < devices_.count(); ++i) {
    for (const DeviceInfo::Backend &backend : devices_[i]->backends_) {
      if (backend.unique_id_ == id) return devices_[i];
    }
  }

  return nullptr;

}

DeviceInfo *DeviceManager::FindDeviceByUrl(const QList<QUrl> &urls) const {

  if (urls.isEmpty()) return nullptr;

  for (int i = 0; i < devices_.count(); ++i) {
    for (const DeviceInfo::Backend &backend : devices_[i]->backends_) {
      if (!backend.lister_) continue;

      QList<QUrl> device_urls = backend.lister_->MakeDeviceUrls(backend.unique_id_);
      for (const QUrl &url : device_urls) {
        if (urls.contains(url)) return devices_[i];
      }
    }
  }

  return nullptr;

}

DeviceInfo *DeviceManager::FindEquivalentDevice(DeviceInfo *info) const {

  for (const DeviceInfo::Backend &backend : info->backends_) {
    DeviceInfo *match = FindDeviceById(backend.unique_id_);
    if (match) return match;
  }
  return nullptr;

}

void DeviceManager::PhysicalDeviceAdded(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());
  if (!lister) return;

  qLog(Info) << "Device added:" << id << lister->DeviceUniqueIDs();

  // Do we have this device already?
  DeviceInfo *info = FindDeviceById(id);
  if (info) {
    for (int backend_index = 0 ; backend_index < info->backends_.count() ; ++backend_index) {
      if (info->backends_[backend_index].unique_id_ == id) {
        info->backends_[backend_index].lister_ = lister;
        break;
      }
    }
    QModelIndex idx = ItemToIndex(info);
    if (idx.isValid()) emit dataChanged(idx, idx);
  }
  else {
    // Check if we have another device with the same URL
    info = FindDeviceByUrl(lister->MakeDeviceUrls(id));
    if (info) {
      // Add this device's lister to the existing device
      info->backends_ << DeviceInfo::Backend(lister, id);

      // If the user hasn't saved the device in the DB yet then overwrite the device's name and icon etc.
      if (info->database_id_ == -1 && info->BestBackend() && info->BestBackend()->lister_ == lister) {
        info->friendly_name_ = lister->MakeFriendlyName(id);
        info->size_ = lister->DeviceCapacity(id);
        info->LoadIcon(lister->DeviceIcons(id), info->friendly_name_);
      }
      QModelIndex idx = ItemToIndex(info);
      if (idx.isValid()) emit dataChanged(idx, idx);
    }
    else {
      // It's a completely new device
      info = new DeviceInfo(DeviceInfo::Type_Device, root_);
      info->backends_ << DeviceInfo::Backend(lister, id);
      info->friendly_name_ = lister->MakeFriendlyName(id);
      info->size_ = lister->DeviceCapacity(id);
      info->LoadIcon(lister->DeviceIcons(id), info->friendly_name_);
      beginInsertRows(ItemToIndex(root_), devices_.count(), devices_.count());
      devices_ << info;
      endInsertRows();
    }
  }

}

void DeviceManager::PhysicalDeviceRemoved(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());

  qLog(Info) << "Device removed:" << id;

  DeviceInfo *info = FindDeviceById(id);
  if (!info) return;

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return;

  if (info->database_id_ != -1) {
    // Keep the structure around, but just "disconnect" it
    for (int backend_index = 0 ; backend_index < info->backends_.count() ; ++backend_index) {
      if (info->backends_[backend_index].unique_id_ == id) {
        info->backends_[backend_index].lister_ = nullptr;
        break;
      }
    }

    if (info->device_ && info->device_->lister() == lister) {
      info->device_->Close();
    }

    if (!info->device_) emit DeviceDisconnected(idx);

    emit dataChanged(idx, idx);
  }
  else {
    // If this was the last lister for the device then remove it from the model
    for (int backend_index = 0 ; backend_index < info->backends_.count() ; ++backend_index) {
      if (info->backends_[backend_index].unique_id_ == id) {
        info->backends_.removeAt(backend_index);
        break;
      }
    }

    if (info->backends_.isEmpty()) {
      beginRemoveRows(ItemToIndex(root_), idx.row(), idx.row());
      devices_.removeAll(info);
      root_->Delete(info->row);
      endRemoveRows();
    }
  }

}

void DeviceManager::PhysicalDeviceChanged(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());
  Q_UNUSED(lister);

  DeviceInfo *info = FindDeviceById(id);
  if (!info) return;

  // TODO

}

std::shared_ptr<ConnectedDevice> DeviceManager::Connect(QModelIndex idx) {

  std::shared_ptr<ConnectedDevice> ret;

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return ret;

  return Connect(info);

}

std::shared_ptr<ConnectedDevice> DeviceManager::Connect(DeviceInfo *info) {

  std::shared_ptr<ConnectedDevice> ret;

  if (!info) return ret;
  if (info->device_) {  // Already connected
    return info->device_;
  }

  if (!info->BestBackend() || !info->BestBackend()->lister_) {  // Not physically connected
    return ret;
  }

  if (info->BestBackend()->lister_->DeviceNeedsMount(info->BestBackend()->unique_id_)) {  // Mount the device
    info->BestBackend()->lister_->MountDeviceAsync(info->BestBackend()->unique_id_);
    return ret;
  }

  bool first_time = (info->database_id_ == -1);
  if (first_time) {
    // We haven't stored this device in the database before
    info->database_id_ = backend_->AddDevice(info->SaveToDb());
  }

  // Get the device URLs
  QList<QUrl> urls = info->BestBackend()->lister_->MakeDeviceUrls(info->BestBackend()->unique_id_);
  if (urls.isEmpty()) return ret;

  // Take the first URL that we have a handler for
  QUrl device_url;
  for (const QUrl &url : urls) {
    qLog(Info) << "Connecting" << url;

    // Find a device class for this URL's scheme
    if (device_classes_.contains(url.scheme())) {
      device_url = url;
      break;
    }

    // If we get here it means that this URL scheme wasn't supported.
    // If it was "ipod" or "mtp" then the user compiled out support and the device won't work properly.
    if (url.scheme() == "mtp" || url.scheme() == "gphoto2") {
      if (QMessageBox::critical(nullptr, tr("This device will not work properly"),
          tr("This is an MTP device, but you compiled Strawberry without libmtp support.") + "  " +
          tr("If you continue, this device will work slowly and songs copied to it may not work."),
              QMessageBox::Abort, QMessageBox::Ignore) == QMessageBox::Abort)
        return ret;
    }

    if (url.scheme() == "ipod") {
      if (QMessageBox::critical(nullptr, tr("This device will not work properly"),
          tr("This is an iPod, but you compiled Strawberry without libgpod support.") + "  " +
          tr("If you continue, this device will work slowly and songs copied to it may not work."),
              QMessageBox::Abort, QMessageBox::Ignore) == QMessageBox::Abort)
        return ret;
    }
  }

  if (device_url.isEmpty()) {
    // Munge the URL list into a string list
    QStringList url_strings;
    for (const QUrl &url : urls) {
      url_strings << url.toString();
    }

    app_->AddError(tr("This type of device is not supported: %1").arg(url_strings.join(", ")));
    return ret;
  }

  QMetaObject meta_object = device_classes_.value(device_url.scheme());
  QObject *instance = meta_object.newInstance(
      Q_ARG(QUrl, device_url),
      Q_ARG(DeviceLister*, info->BestBackend()->lister_),
      Q_ARG(QString, info->BestBackend()->unique_id_),
      Q_ARG(DeviceManager*, this),
      Q_ARG(Application*, app_),
      Q_ARG(int, info->database_id_),
      Q_ARG(bool, first_time));

  ret.reset(static_cast<ConnectedDevice*>(instance));

  if (!ret) {
    qLog(Warning) << "Could not create device for" << device_url.toString();
    return ret;
  }

  bool result = ret->Init();
  if (!result) {
    qLog(Warning) << "Could not connect to device" << device_url.toString();
    return ret;
  }
  info->device_ = ret;

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return ret;

  emit dataChanged(idx, idx);

  connect(info->device_.get(), SIGNAL(TaskStarted(int)), SLOT(DeviceTaskStarted(int)));
  connect(info->device_.get(), SIGNAL(SongCountUpdated(int)), SLOT(DeviceSongCountUpdated(int)));
  connect(info->device_.get(), SIGNAL(ConnectFinished(QString, bool)), SLOT(DeviceConnectFinished(QString, bool)));
  connect(info->device_.get(), SIGNAL(CloseFinished(QString)), SLOT(DeviceCloseFinished(QString)));
  ret->ConnectAsync();
  return ret;

}

void DeviceManager::DeviceConnectFinished(const QString &id, bool success) {

  DeviceInfo *info = FindDeviceById(id);
  if (!info) return;

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return;

  if (success) {
    emit DeviceConnected(idx);
  }
  else {
    info->device_->Close();
  }

}

void DeviceManager::DeviceCloseFinished(const QString &id) {

  DeviceInfo *info = FindDeviceById(id);
  if (!info) return;

  info->device_.reset();

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return;

  emit DeviceDisconnected(idx);
  emit dataChanged(idx, idx);

  if (info->unmount_ && info->BestBackend() && info->BestBackend()->lister_) {
    info->BestBackend()->lister_->UnmountDeviceAsync(info->BestBackend()->unique_id_);
  }

  if (info->forget_) {
    RemoveFromDB(info, idx);
  }

}

DeviceInfo *DeviceManager::GetDevice(QModelIndex idx) const {

  DeviceInfo *info = IndexToItem(idx);
  return info;

}

std::shared_ptr<ConnectedDevice> DeviceManager::GetConnectedDevice(QModelIndex idx) const {

  std::shared_ptr<ConnectedDevice> ret;
  DeviceInfo *info = IndexToItem(idx);
  if (!info) return ret;
  return info->device_;

}

std::shared_ptr<ConnectedDevice> DeviceManager::GetConnectedDevice(DeviceInfo *info) const {

  std::shared_ptr<ConnectedDevice> ret;
  if (!info) return ret;
  return info->device_;

}

int DeviceManager::GetDatabaseId(QModelIndex idx) const {

  if (!idx.isValid()) return -1;

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return -1;
  return info->database_id_;

}

DeviceLister *DeviceManager::GetLister(QModelIndex idx) const {

  if (!idx.isValid()) return nullptr;

  DeviceInfo *info = IndexToItem(idx);
  if (!info || !info->BestBackend()) return nullptr;
  return info->BestBackend()->lister_;

}

void DeviceManager::Disconnect(DeviceInfo *info, QModelIndex idx) {

  Q_UNUSED(idx);

  info->device_->Close();

}

void DeviceManager::Forget(QModelIndex idx) {

  if (!idx.isValid()) return;

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return;

  if (info->database_id_ == -1) return;

  if (info->device_) {
    info->forget_ = true;
    Disconnect(info, idx);
  }
  else {
    RemoveFromDB(info, idx);
  }

}

void DeviceManager::RemoveFromDB(DeviceInfo *info, QModelIndex idx) {

  backend_->RemoveDevice(info->database_id_);
  info->database_id_ = -1;

  if (!info->BestBackend() || !info->BestBackend()->lister_) {  // It's not attached any more so remove it from the list
    beginRemoveRows(ItemToIndex(root_), idx.row(), idx.row());
    devices_.removeAll(info);
    root_->Delete(info->row);
    endRemoveRows();
  }
  else {  // It's still attached, set the name and icon back to what they were originally
    const QString id = info->BestBackend()->unique_id_;

    info->friendly_name_ = info->BestBackend()->lister_->MakeFriendlyName(id);
    info->LoadIcon(info->BestBackend()->lister_->DeviceIcons(id), info->friendly_name_);
    dataChanged(idx, idx);
  }

}

void DeviceManager::SetDeviceOptions(QModelIndex idx, const QString &friendly_name, const QString &icon_name, MusicStorage::TranscodeMode mode, Song::FileType format) {

  if (!idx.isValid()) return;

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return;

  info->friendly_name_ = friendly_name;
  info->LoadIcon(QVariantList() << icon_name, friendly_name);
  info->transcode_mode_ = mode;
  info->transcode_format_ = format;

  emit dataChanged(idx, idx);

  if (info->database_id_ != -1)
    backend_->SetDeviceOptions(info->database_id_, friendly_name, icon_name, mode, format);

}

void DeviceManager::DeviceTaskStarted(int id) {

  ConnectedDevice *device = qobject_cast<ConnectedDevice*>(sender());
  if (!device) return;

  for (int i = 0; i < devices_.count(); ++i) {
    DeviceInfo *info = devices_[i];
    if (info->device_.get() == device) {
      QModelIndex index = ItemToIndex(info);
      if (!index.isValid()) continue;
      active_tasks_[id] = index;
      info->task_percentage_ = 0;
      emit dataChanged(index, index);
      return;
    }
  }

}

void DeviceManager::TasksChanged() {

  QList<TaskManager::Task> tasks = app_->task_manager()->GetTasks();
  QList<QPersistentModelIndex> finished_tasks = active_tasks_.values();

  for (const TaskManager::Task &task : tasks) {
    if (!active_tasks_.contains(task.id)) continue;

    QPersistentModelIndex idx = active_tasks_[task.id];
    if (!idx.isValid()) continue;

    DeviceInfo *info = IndexToItem(idx);
    if (task.progress_max)
      info->task_percentage_ = float(task.progress) / task.progress_max * 100;
    else
      info->task_percentage_ = 0;

    emit dataChanged(idx, idx);
    finished_tasks.removeAll(idx);

  }

  for (const QPersistentModelIndex &idx : finished_tasks) {

    if (!idx.isValid()) continue;

    DeviceInfo *info = IndexToItem(idx);
    if (!info) continue;

    info->task_percentage_ = -1;
    emit dataChanged(idx, idx);

    active_tasks_.remove(active_tasks_.key(idx));
  }

}

void DeviceManager::UnmountAsync(QModelIndex idx) {
  Q_ASSERT(QMetaObject::invokeMethod(this, "Unmount", Q_ARG(QModelIndex, idx)));
}

void DeviceManager::Unmount(QModelIndex idx) {

  if (!idx.isValid()) return;

  DeviceInfo *info = IndexToItem(idx);
  if (!info) return;

  if (info->database_id_ != -1 && !info->device_) return;

  if (info->device_) {
    info->unmount_ = true;
    Disconnect(info, idx);
  }
  else if (info->BestBackend() && info->BestBackend()->lister_) {
    info->BestBackend()->lister_->UnmountDeviceAsync(info->BestBackend()->unique_id_);
  }

}

void DeviceManager::DeviceSongCountUpdated(int count) {

  Q_UNUSED(count);

  ConnectedDevice *device = qobject_cast<ConnectedDevice*>(sender());
  if (!device) return;

  DeviceInfo *info = FindDeviceById(device->unique_id());
  if (!info) return;

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return;

  emit dataChanged(idx, idx);

}

void DeviceManager::LazyPopulate(DeviceInfo *parent, bool signal) {

  Q_UNUSED(signal);
  if (parent->lazy_loaded) return;
  parent->lazy_loaded = true;

}

QString DeviceManager::DeviceNameByID(const QString &unique_id) {

  DeviceInfo *info = FindDeviceById(unique_id);
  if (!info) return QString();

  QModelIndex idx = ItemToIndex(info);
  if (!idx.isValid()) return QString();

  return data(idx, DeviceManager::Role_FriendlyName).toString();

}
