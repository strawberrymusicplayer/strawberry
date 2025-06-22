/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <functional>
#include <memory>

#include <QtGlobal>
#include <QApplication>
#include <QObject>
#include <QMetaObject>
#include <QThread>
#include <QtConcurrentRun>
#include <QAbstractItemModel>
#include <QDir>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QPixmap>
#include <QPainter>
#include <QMessageBox>
#include <QPushButton>

#include "devicemanager.h"

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "core/taskmanager.h"
#include "core/simpletreemodel.h"
#include "utilities/strutils.h"
#include "filesystemdevice.h"
#include "connecteddevice.h"
#include "devicelister.h"
#include "devicedatabasebackend.h"
#include "devicestatefiltermodel.h"
#include "deviceinfo.h"

#ifdef HAVE_GIO
#  include "giolister.h"
#endif
#ifdef HAVE_AUDIOCD
#  include "cddalister.h"
#  include "cddadevice.h"
#endif
#ifdef HAVE_UDISKS2
#  include "udisks2lister.h"
#endif
#ifdef HAVE_MTP
#  include "mtpdevice.h"
#endif
#ifdef Q_OS_MACOS
#  include "macosdevicelister.h"
#endif
#ifdef HAVE_GPOD
#  include "gpoddevice.h"
#endif

using namespace Qt::Literals::StringLiterals;
using std::make_unique;

const int DeviceManager::kDeviceIconSize = 32;
const int DeviceManager::kDeviceIconOverlaySize = 16;

DeviceManager::DeviceManager(const SharedPtr<TaskManager> task_manager,
                             const SharedPtr<Database> database,
                             const SharedPtr<TagReaderClient> tagreader_client,
                             const SharedPtr<AlbumCoverLoader> albumcover_loader,
                             QObject *parent)
    : SimpleTreeModel<DeviceInfo>(new DeviceInfo(this), parent),
      task_manager_(task_manager),
      database_(database),
      tagreader_client_(tagreader_client),
      albumcover_loader_(albumcover_loader),
      not_connected_overlay_(IconLoader::Load(u"edit-delete"_s)) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  thread_pool_.setMaxThreadCount(1);
  QObject::connect(&*task_manager, &TaskManager::TasksChanged, this, &DeviceManager::TasksChanged);

  // Create the backend in the database thread
  backend_ = make_unique<DeviceDatabaseBackend>();
  backend_->moveToThread(database->thread());
  backend_->Init(database);

  QObject::connect(this, &DeviceManager::DevicesLoaded, this, &DeviceManager::AddDevicesFromDB);

  // This reads from the database and contents on the database mutex, which can be very slow on startup.
  (void)QtConcurrent::run(&thread_pool_, &DeviceManager::LoadAllDevices, this);

  // This proxy model only shows connected devices
  connected_devices_model_ = new DeviceStateFilterModel(this);
  connected_devices_model_->setSourceModel(this);

// CD devices are detected via the DiskArbitration framework instead on MacOs.
#if defined(HAVE_AUDIOCD) && !defined(Q_OS_MACOS)
  AddLister(new CDDALister);
#endif
#ifdef HAVE_UDISKS2
  AddLister(new Udisks2Lister);
#endif
#ifdef HAVE_GIO
  AddLister(new GioLister);
#endif
#ifdef Q_OS_MACOS
  AddLister(new MacOsDeviceLister);
#endif

#ifdef HAVE_AUDIOCD
  AddDeviceClass<CDDADevice>();
#endif

  AddDeviceClass<FilesystemDevice>();

#ifdef HAVE_GPOD
  AddDeviceClass<GPodDevice>();
#endif

#ifdef HAVE_MTP
  AddDeviceClass<MtpDevice>();
#endif

}

DeviceManager::~DeviceManager() {

  for (DeviceLister *lister : std::as_const(listers_)) {
    lister->ShutDown();
    delete lister;
  }
  listers_.clear();

  delete root_;
  root_ = nullptr;

}

void DeviceManager::Exit() {
  CloseDevices();
}

void DeviceManager::CloseDevices() {

  for (DeviceInfo *device_info : std::as_const(devices_)) {
    if (!device_info->device_) continue;
    if (wait_for_exit_.contains(&*device_info->device_)) continue;
    wait_for_exit_ << &*device_info->device_;
    QObject::connect(&*device_info->device_, &ConnectedDevice::destroyed, this, &DeviceManager::DeviceDestroyed);
    device_info->device_->Close();
  }
  if (wait_for_exit_.isEmpty()) CloseListers();

}

void DeviceManager::CloseListers() {

  for (DeviceLister *lister : std::as_const(listers_)) {
    if (wait_for_exit_.contains(lister)) continue;
    wait_for_exit_ << lister;
    QObject::connect(lister, &DeviceLister::ExitFinished, this, &DeviceManager::ListerClosed);
    lister->ExitAsync();
  }
  if (wait_for_exit_.isEmpty()) CloseBackend();

}

void DeviceManager::CloseBackend() {

  if (!backend_ || wait_for_exit_.contains(&*backend_)) return;
  wait_for_exit_ << &*backend_;
  QObject::connect(&*backend_, &DeviceDatabaseBackend::ExitFinished, this, &DeviceManager::BackendClosed);
  backend_->ExitAsync();

}

void DeviceManager::BackendClosed() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully closed.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void DeviceManager::ListerClosed() {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());
  if (!lister) return;

  QObject::disconnect(lister, nullptr, this, nullptr);
  qLog(Debug) << lister << "successfully closed.";
  wait_for_exit_.removeAll(lister);

  if (wait_for_exit_.isEmpty()) CloseBackend();

}

void DeviceManager::DeviceDestroyed() {

  ConnectedDevice *connected_device = static_cast<ConnectedDevice*>(sender());
  if (!wait_for_exit_.contains(connected_device) || !backend_) return;

  wait_for_exit_.removeAll(connected_device);
  if (wait_for_exit_.isEmpty()) CloseListers();

}

void DeviceManager::LoadAllDevices() {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const DeviceDatabaseBackend::DeviceList devices = backend_->GetAllDevices();

  Q_EMIT DevicesLoaded(devices);

  // This is done in a concurrent thread so close the unique DB connection.
  backend_->Close();

}

void DeviceManager::AddDevicesFromDB(const DeviceDatabaseBackend::DeviceList &devices) {

  for (const DeviceDatabaseBackend::Device &device : devices) {
    const QStringList unique_ids = device.unique_id_.split(u',');
    DeviceInfo *device_info = FindEquivalentDevice(unique_ids);
    if (device_info && device_info->database_id_ == -1) {
      qLog(Info) << "Database device linked to physical device:" << device.friendly_name_;
      device_info->database_id_ = device.id_;
      device_info->icon_name_ = device.icon_name_;
      device_info->InitIcon();
      const QModelIndex idx = ItemToIndex(device_info);
      if (idx.isValid()) {
        Q_EMIT dataChanged(idx, idx);
      }
    }
    else {
      qLog(Info) << "Database device:" << device.friendly_name_;
      device_info = new DeviceInfo(DeviceInfo::Type::Device, root_);
      device_info->InitFromDb(device);
      beginInsertRows(ItemToIndex(root_), static_cast<int>(devices_.count()), static_cast<int>(devices_.count()));
      devices_ << device_info;
      endInsertRows();
    }
  }

}

QVariant DeviceManager::data(const QModelIndex &idx, int role) const {

  if (!idx.isValid() || idx.column() != 0) return QVariant();

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return QVariant();

  switch (role) {
    case Qt::DisplayRole:{
      QString text;
      if (!device_info->friendly_name_.isEmpty()) {
        text = device_info->friendly_name_;
      }
      else if (device_info->BestBackend()) {
        text = device_info->BestBackend()->unique_id_;
      }

      if (device_info->size_ > 0) {
        text = text + QStringLiteral(" (%1)").arg(Utilities::PrettySize(device_info->size_));
      }
      return text;
    }

    case Qt::DecorationRole:{
      QPixmap pixmap = device_info->icon_.pixmap(kDeviceIconSize);

      if (device_info->backends_.isEmpty() || !device_info->BestBackend() || !device_info->BestBackend()->lister_) {
        // Disconnected but remembered
        QPainter p(&pixmap);
        p.drawPixmap(kDeviceIconSize - kDeviceIconOverlaySize, kDeviceIconSize - kDeviceIconOverlaySize, not_connected_overlay_.pixmap(kDeviceIconOverlaySize));
      }

      return pixmap;
    }

    case Role_FriendlyName:
      return device_info->friendly_name_;

    case Role_UniqueId:
      if (!device_info->BestBackend()) return QString();
      return device_info->BestBackend()->unique_id_;

    case Role_IconName:
      return device_info->icon_name_;

    case Role_Capacity:
    case MusicStorage::Role_Capacity:
      return device_info->size_;

    case Role_FreeSpace:
    case MusicStorage::Role_FreeSpace:
      return ((device_info->BestBackend() && device_info->BestBackend()->lister_) ? device_info->BestBackend()->lister_->DeviceFreeSpace(device_info->BestBackend()->unique_id_) : QVariant());

    case Role_State:
      if (device_info->device_) return QVariant::fromValue(State::Connected);
      if (device_info->BestBackend() && device_info->BestBackend()->lister_) {
        if (device_info->BestBackend()->lister_->DeviceNeedsMount(device_info->BestBackend()->unique_id_)) return QVariant::fromValue(State::NotMounted);
        return QVariant::fromValue(State::NotConnected);
      }
      return QVariant::fromValue(State::Remembered);

    case Role_UpdatingPercentage:
      if (device_info->task_percentage_ == -1) return QVariant();
      return device_info->task_percentage_;

    case MusicStorage::Role_Storage:
      if (!device_info->device_ && device_info->database_id_ != -1) {
        const_cast<DeviceManager*>(this)->Connect(device_info);
      }
      if (!device_info->device_) return QVariant();
      return QVariant::fromValue<SharedPtr<MusicStorage>>(device_info->device_);

    case MusicStorage::Role_StorageForceConnect:
      if (!device_info->BestBackend()) return QVariant();
      if (!device_info->device_) {
        if (device_info->database_id_ == -1 && !device_info->BestBackend()->lister_->DeviceNeedsMount(device_info->BestBackend()->unique_id_)) {
          if (device_info->BestBackend()->lister_->AskForScan(device_info->BestBackend()->unique_id_)) {
            ScopedPtr<QMessageBox> dialog(new QMessageBox(QMessageBox::Information, tr("Connect device"), tr("This is the first time you have connected this device.  Strawberry will now scan the device to find music files - this may take some time."), QMessageBox::Cancel));
            QPushButton *pushbutton = dialog->addButton(tr("Connect device"), QMessageBox::AcceptRole);
            dialog->exec();
            if (dialog->clickedButton() != pushbutton) return QVariant();
          }
        }
        const_cast<DeviceManager*>(this)->Connect(device_info);
      }
      if (!device_info->device_) return QVariant();
      return QVariant::fromValue<SharedPtr<MusicStorage>>(device_info->device_);

    case Role_MountPath:{
      if (!device_info->device_) return QVariant();

      QString ret = device_info->device_->url().path();
#ifdef Q_OS_WIN32
      if (ret.startsWith(u'/')) ret.remove(0, 1);
#endif
      return QDir::toNativeSeparators(ret);
    }

    case Role_TranscodeMode:
      return static_cast<int>(device_info->transcode_mode_);

    case Role_TranscodeFormat:
      return static_cast<int>(device_info->transcode_format_);

    case Role_SongCount:
      if (!device_info->device_) return QVariant();
      return device_info->device_->song_count();

    case Role_CopyMusic:
      if (device_info->BestBackend() && device_info->BestBackend()->lister_) return device_info->BestBackend()->lister_->CopyMusic();
      else return false;

    default:
      return QVariant();
  }

}

void DeviceManager::AddLister(DeviceLister *lister) {

  listers_ << lister;
  QObject::connect(lister, &DeviceLister::DeviceAdded, this, &DeviceManager::PhysicalDeviceAdded);
  QObject::connect(lister, &DeviceLister::DeviceRemoved, this, &DeviceManager::PhysicalDeviceRemoved);
  QObject::connect(lister, &DeviceLister::DeviceChanged, this, &DeviceManager::PhysicalDeviceChanged);

  lister->Start();

}

DeviceInfo *DeviceManager::FindDeviceById(const QString &id) const {

  for (int i = 0; i < devices_.count(); ++i) {
    for (const DeviceInfo::Backend &backend : std::as_const(devices_[i]->backends_)) {
      if (backend.unique_id_ == id) {
        return devices_[i];
      }
    }
  }

  return nullptr;

}

DeviceInfo *DeviceManager::FindDeviceByUrl(const QList<QUrl> &urls) const {

  if (urls.isEmpty()) return nullptr;

  for (int i = 0; i < devices_.count(); ++i) {
    for (const DeviceInfo::Backend &backend : std::as_const(devices_[i]->backends_)) {
      if (!backend.lister_) continue;
      const QList<QUrl> device_urls = backend.lister_->MakeDeviceUrls(backend.unique_id_);
      for (const QUrl &url : device_urls) {
        if (urls.contains(url)) {
          return devices_[i];
        }
      }
    }
  }

  return nullptr;

}

DeviceInfo *DeviceManager::FindEquivalentDevice(const QStringList &unique_ids) const {

  for (const QString &unique_id : unique_ids) {
    DeviceInfo *device_info_match = FindDeviceById(unique_id);
    if (device_info_match) {
      return device_info_match;
    }
  }

  return nullptr;

}

void DeviceManager::PhysicalDeviceAdded(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());
  if (!lister) return;

  qLog(Info) << "Device added:" << id << lister->DeviceUniqueIDs();

  // Do we have this device already?
  DeviceInfo *device_info = FindDeviceById(id);
  if (device_info) {
    for (int backend_index = 0; backend_index < device_info->backends_.count(); ++backend_index) {
      if (device_info->backends_[backend_index].unique_id_ == id) {
        device_info->backends_[backend_index].lister_ = lister;
        break;
      }
    }
    QModelIndex idx = ItemToIndex(device_info);
    if (idx.isValid()) Q_EMIT dataChanged(idx, idx);
  }
  else {
    // Check if we have another device with the same URL
    device_info = FindDeviceByUrl(lister->MakeDeviceUrls(id));
    if (device_info) {
      // Add this device's lister to the existing device
      device_info->backends_ << DeviceInfo::Backend(lister, id);

      // If the user hasn't saved the device in the DB yet then overwrite the device's name and icon etc.
      if (device_info->database_id_ == -1 && device_info->BestBackend() && device_info->BestBackend()->lister_ == lister) {
        device_info->friendly_name_ = lister->MakeFriendlyName(id);
        device_info->size_ = lister->DeviceCapacity(id);
        device_info->LoadIcon(lister->DeviceIcons(id), device_info->friendly_name_);
      }
      QModelIndex idx = ItemToIndex(device_info);
      if (idx.isValid()) Q_EMIT dataChanged(idx, idx);
    }
    else {
      // It's a completely new device
      device_info = new DeviceInfo(DeviceInfo::Type::Device, root_);
      device_info->backends_ << DeviceInfo::Backend(lister, id);
      device_info->friendly_name_ = lister->MakeFriendlyName(id);
      device_info->size_ = lister->DeviceCapacity(id);
      device_info->LoadIcon(lister->DeviceIcons(id), device_info->friendly_name_);
      beginInsertRows(ItemToIndex(root_), static_cast<int>(devices_.count()), static_cast<int>(devices_.count()));
      devices_ << device_info;
      endInsertRows();
    }
  }

}

void DeviceManager::PhysicalDeviceRemoved(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());

  qLog(Info) << "Device removed:" << id;

  DeviceInfo *device_info = FindDeviceById(id);
  if (!device_info) return;

  const QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return;

  if (device_info->database_id_ != -1) {
    // Keep the structure around, but just "disconnect" it
    for (int backend_index = 0; backend_index < device_info->backends_.count(); ++backend_index) {
      if (device_info->backends_[backend_index].unique_id_ == id) {
        device_info->backends_[backend_index].lister_ = nullptr;
        break;
      }
    }

    if (device_info->device_ && device_info->device_->lister() == lister) {
      device_info->device_->Close();
    }

    if (!device_info->device_) Q_EMIT DeviceDisconnected(idx);

    Q_EMIT dataChanged(idx, idx);
  }
  else {
    // If this was the last lister for the device then remove it from the model
    for (int backend_index = 0; backend_index < device_info->backends_.count(); ++backend_index) {
      if (device_info->backends_[backend_index].unique_id_ == id) {
        device_info->backends_.removeAt(backend_index);
        break;
      }
    }

    if (device_info->backends_.isEmpty()) {
      beginRemoveRows(ItemToIndex(root_), idx.row(), idx.row());
      devices_.removeAll(device_info);
      root_->Delete(device_info->row);
      endRemoveRows();
    }
  }

}

void DeviceManager::PhysicalDeviceChanged(const QString &id) {

  DeviceLister *lister = qobject_cast<DeviceLister*>(sender());
  Q_UNUSED(lister);

  DeviceInfo *device_info = FindDeviceById(id);
  if (!device_info) return;

  // TODO

}

SharedPtr<ConnectedDevice> DeviceManager::Connect(const QModelIndex &idx) {

  SharedPtr<ConnectedDevice> ret;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return ret;

  return Connect(device_info);

}

SharedPtr<ConnectedDevice> DeviceManager::Connect(DeviceInfo *device_info) {

  if (!device_info) {
    return SharedPtr<ConnectedDevice>();
  }

  if (device_info->device_) {  // Already connected
    return device_info->device_;
  }

  if (!device_info->BestBackend() || !device_info->BestBackend()->lister_) {  // Not physically connected
    return SharedPtr<ConnectedDevice>();
  }

  if (device_info->BestBackend()->lister_->DeviceNeedsMount(device_info->BestBackend()->unique_id_)) {  // Mount the device
    device_info->BestBackend()->lister_->MountDeviceAsync(device_info->BestBackend()->unique_id_);
    return SharedPtr<ConnectedDevice>();
  }

  const bool first_time = device_info->database_id_ == -1;
  if (first_time) {
    // We haven't stored this device in the database before
    device_info->database_id_ = backend_->AddDevice(device_info->SaveToDb());
  }

  // Get the device URLs
  const QList<QUrl> urls = device_info->BestBackend()->lister_->MakeDeviceUrls(device_info->BestBackend()->unique_id_);
  if (urls.isEmpty()) return SharedPtr<ConnectedDevice>();

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
    if (url.scheme() == "mtp"_L1 || url.scheme() == "gphoto2"_L1) {
      if (QMessageBox::critical(nullptr, tr("This device will not work properly"),
          tr("This is an MTP device, but you compiled Strawberry without libmtp support.") + u"  "_s +
          tr("If you continue, this device will work slowly and songs copied to it may not work."),
              QMessageBox::Abort, QMessageBox::Ignore) == QMessageBox::Abort)
        return SharedPtr<ConnectedDevice>();
    }

    if (url.scheme() == "ipod"_L1) {
      if (QMessageBox::critical(nullptr, tr("This device will not work properly"),
          tr("This is an iPod, but you compiled Strawberry without libgpod support.") + "  "_L1 +
          tr("If you continue, this device will work slowly and songs copied to it may not work."),
              QMessageBox::Abort, QMessageBox::Ignore) == QMessageBox::Abort)
        return SharedPtr<ConnectedDevice>();
    }
  }

  if (device_url.isEmpty()) {
    // Munge the URL list into a string list
    QStringList url_strings;
    url_strings.reserve(urls.count());
    for (const QUrl &url : urls) {
      url_strings << url.toString();
    }

    Q_EMIT DeviceError(tr("This type of device is not supported: %1").arg(url_strings.join(", "_L1)));
    return SharedPtr<ConnectedDevice>();
  }

  QMetaObject meta_object = device_classes_.value(device_url.scheme());
  QObject *instance = meta_object.newInstance(
      Q_ARG(QUrl, device_url),
      Q_ARG(DeviceLister*, device_info->BestBackend()->lister_),
      Q_ARG(QString, device_info->BestBackend()->unique_id_),
      Q_ARG(DeviceManager*, this),
      Q_ARG(SharedPtr<TaskManager>, task_manager_),
      Q_ARG(SharedPtr<Database>, database_),
      Q_ARG(SharedPtr<TagReaderClient>, tagreader_client_),
      Q_ARG(SharedPtr<AlbumCoverLoader>, albumcover_loader_),
      Q_ARG(int, device_info->database_id_),
      Q_ARG(bool, first_time));

  SharedPtr<ConnectedDevice> connected_device = SharedPtr<ConnectedDevice>(qobject_cast<ConnectedDevice*>(instance));

  if (!connected_device) {
    qLog(Warning) << "Could not create device for" << device_url.toString();
    return connected_device;
  }

  bool result = connected_device->Init();
  if (!result) {
    qLog(Warning) << "Could not connect to device" << device_url.toString();
    return connected_device;
  }
  device_info->device_ = connected_device;

  QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return connected_device;

  Q_EMIT dataChanged(idx, idx);

  QObject::connect(&*device_info->device_, &ConnectedDevice::TaskStarted, this, &DeviceManager::DeviceTaskStarted);
  QObject::connect(&*device_info->device_, &ConnectedDevice::SongCountUpdated, this, &DeviceManager::DeviceSongCountUpdated);
  QObject::connect(&*device_info->device_, &ConnectedDevice::DeviceConnectFinished, this, &DeviceManager::DeviceConnectFinished);
  QObject::connect(&*device_info->device_, &ConnectedDevice::DeviceCloseFinished, this, &DeviceManager::DeviceCloseFinished);
  QObject::connect(&*device_info->device_, &ConnectedDevice::Error, this, &DeviceManager::DeviceError);

  connected_device->ConnectAsync();

  return connected_device;

}

void DeviceManager::DeviceConnectFinished(const QString &id, const bool success) {

  DeviceInfo *device_info = FindDeviceById(id);
  if (!device_info) return;

  QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return;

  if (success) {
    Q_EMIT DeviceConnected(idx);
  }
  else {
    device_info->device_->Close();
  }

}

void DeviceManager::DeviceCloseFinished(const QString &id) {

  DeviceInfo *device_info = FindDeviceById(id);
  if (!device_info) return;

  device_info->device_.reset();

  QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return;

  Q_EMIT DeviceDisconnected(idx);
  Q_EMIT dataChanged(idx, idx);

  if (device_info->unmount_ && device_info->BestBackend() && device_info->BestBackend()->lister_) {
    device_info->BestBackend()->lister_->UnmountDeviceAsync(device_info->BestBackend()->unique_id_);
  }

  if (device_info->forget_) {
    RemoveFromDB(device_info, idx);
  }

}

DeviceInfo *DeviceManager::GetDevice(const QModelIndex &idx) const {

  DeviceInfo *device_info = IndexToItem(idx);
  return device_info;

}

SharedPtr<ConnectedDevice> DeviceManager::GetConnectedDevice(const QModelIndex &idx) const {

  SharedPtr<ConnectedDevice> connected_device;
  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return connected_device;
  return device_info->device_;

}

SharedPtr<ConnectedDevice> DeviceManager::GetConnectedDevice(DeviceInfo *device_info) const {

  SharedPtr<ConnectedDevice> connected_device;
  if (!device_info) return connected_device;
  return device_info->device_;

}

int DeviceManager::GetDatabaseId(const QModelIndex &idx) const {

  if (!idx.isValid()) return -1;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return -1;
  return device_info->database_id_;

}

DeviceLister *DeviceManager::GetLister(const QModelIndex &idx) const {

  if (!idx.isValid()) return nullptr;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info || !device_info->BestBackend()) return nullptr;
  return device_info->BestBackend()->lister_;

}

void DeviceManager::Disconnect(DeviceInfo *device_info, const QModelIndex &idx) {

  Q_UNUSED(idx);

  device_info->device_->Close();

}

void DeviceManager::Forget(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return;

  if (device_info->database_id_ == -1) return;

  if (device_info->device_) {
    device_info->forget_ = true;
    Disconnect(device_info, idx);
  }
  else {
    RemoveFromDB(device_info, idx);
  }

}

void DeviceManager::RemoveFromDB(DeviceInfo *device_info, const QModelIndex &idx) {

  backend_->RemoveDevice(device_info->database_id_);
  device_info->database_id_ = -1;

  if (!device_info->BestBackend() || !device_info->BestBackend()->lister_) {  // It's not attached any more so remove it from the list
    beginRemoveRows(ItemToIndex(root_), idx.row(), idx.row());
    devices_.removeAll(device_info);
    root_->Delete(device_info->row);
    endRemoveRows();
  }
  else {  // It's still attached, set the name and icon back to what they were originally
    const QString id = device_info->BestBackend()->unique_id_;

    device_info->friendly_name_ = device_info->BestBackend()->lister_->MakeFriendlyName(id);
    device_info->LoadIcon(device_info->BestBackend()->lister_->DeviceIcons(id), device_info->friendly_name_);
    Q_EMIT dataChanged(idx, idx);
  }

}

void DeviceManager::SetDeviceOptions(const QModelIndex &idx, const QString &friendly_name, const QString &icon_name, const MusicStorage::TranscodeMode mode, const Song::FileType format) {

  if (!idx.isValid()) return;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return;

  device_info->friendly_name_ = friendly_name;
  device_info->LoadIcon(QVariantList() << icon_name, friendly_name);
  device_info->transcode_mode_ = mode;
  device_info->transcode_format_ = format;

  Q_EMIT dataChanged(idx, idx);

  if (device_info->database_id_ != -1) {
    backend_->SetDeviceOptions(device_info->database_id_, friendly_name, icon_name, mode, format);
  }

}

void DeviceManager::DeviceTaskStarted(const int id) {

  ConnectedDevice *device = qobject_cast<ConnectedDevice*>(sender());
  if (!device) return;

  for (int i = 0; i < devices_.count(); ++i) {
    DeviceInfo *device_info = devices_.value(i);
    if (device_info->device_ && &*device_info->device_ == device) {
      QModelIndex index = ItemToIndex(device_info);
      if (!index.isValid()) continue;
      active_tasks_[id] = index;
      device_info->task_percentage_ = 0;
      Q_EMIT dataChanged(index, index);
      return;
    }
  }

}

void DeviceManager::TasksChanged() {

  const QList<TaskManager::Task> tasks = task_manager_->GetTasks();
  QList<QPersistentModelIndex> finished_tasks = active_tasks_.values();

  for (const TaskManager::Task &task : tasks) {
    if (!active_tasks_.contains(task.id)) continue;

    const QPersistentModelIndex idx = active_tasks_.value(task.id);
    if (!idx.isValid()) continue;

    DeviceInfo *device_info = IndexToItem(idx);
    if (task.progress_max) {
      device_info->task_percentage_ = static_cast<int>(static_cast<float>(task.progress) / static_cast<float>(task.progress_max) * 100);
    }
    else {
      device_info->task_percentage_ = 0;
    }

    Q_EMIT dataChanged(idx, idx);
    finished_tasks.removeAll(idx);

  }

  for (const QPersistentModelIndex &idx : std::as_const(finished_tasks)) {

    if (!idx.isValid()) continue;

    DeviceInfo *device_info = IndexToItem(idx);
    if (!device_info) continue;

    device_info->task_percentage_ = -1;
    Q_EMIT dataChanged(idx, idx);

    active_tasks_.remove(active_tasks_.key(idx));
  }

}

void DeviceManager::UnmountAsync(const QModelIndex &idx) {
  Q_ASSERT(QMetaObject::invokeMethod(this, "Unmount", Q_ARG(QModelIndex, idx)));
}

void DeviceManager::Unmount(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  DeviceInfo *device_info = IndexToItem(idx);
  if (!device_info) return;

  if (device_info->database_id_ != -1 && !device_info->device_) return;

  if (device_info->device_) {
    device_info->unmount_ = true;
    Disconnect(device_info, idx);
  }
  else if (device_info->BestBackend() && device_info->BestBackend()->lister_) {
    device_info->BestBackend()->lister_->UnmountDeviceAsync(device_info->BestBackend()->unique_id_);
  }

}

void DeviceManager::DeviceSongCountUpdated(const int count) {

  Q_UNUSED(count);

  ConnectedDevice *connected_device = qobject_cast<ConnectedDevice*>(sender());
  if (!connected_device) return;

  DeviceInfo *device_info = FindDeviceById(connected_device->unique_id());
  if (!device_info) return;

  QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return;

  Q_EMIT dataChanged(idx, idx);

}

QString DeviceManager::DeviceNameByID(const QString &unique_id) {

  DeviceInfo *device_info = FindDeviceById(unique_id);
  if (!device_info) return QString();

  QModelIndex idx = ItemToIndex(device_info);
  if (!idx.isValid()) return QString();

  return data(idx, DeviceManager::Role_FriendlyName).toString();

}
