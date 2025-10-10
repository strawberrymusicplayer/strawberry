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

#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "config.h"

#include <QObject>
#include <QMetaObject>
#include <QThreadPool>
#include <QAbstractItemModel>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QIcon>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/musicstorage.h"
#include "core/simpletreemodel.h"
#include "collection/collectionmodel.h"
#include "devicedatabasebackend.h"
#include "deviceinfo.h"

class QModelIndex;
class QPersistentModelIndex;

class TaskManager;
class Database;
class TagReaderClient;
class AlbumCoverLoader;
class ConnectedDevice;
class DeviceLister;
class DeviceStateFilterModel;

class DeviceManager : public SimpleTreeModel<DeviceInfo> {
  Q_OBJECT

 public:
  explicit DeviceManager(const SharedPtr<TaskManager> task_manager,
                         const SharedPtr<Database> database,
                         const SharedPtr<TagReaderClient> tagreader_client,
                         const SharedPtr<AlbumCoverLoader> albumcover_loader,
                         QObject *parent = nullptr);

  ~DeviceManager() override;

  enum Role {
    Role_State = CollectionModel::LastRole,
    Role_UniqueId,
    Role_FriendlyName,
    Role_Capacity,
    Role_FreeSpace,
    Role_IconName,
    Role_UpdatingPercentage,
    Role_MountPath,
    Role_TranscodeMode,
    Role_TranscodeFormat,
    Role_SongCount,
    Role_CopyMusic,
    LastRole,
  };

  enum class State {
    Remembered,
    NotMounted,
    NotConnected,
    Connected,
  };

  static const int kDeviceIconSize;
  static const int kDeviceIconOverlaySize;

  void Exit();

  DeviceStateFilterModel *connected_devices_model() const { return connected_devices_model_; }

  // Get info about devices
  int GetDatabaseId(const QModelIndex &idx) const;
  DeviceLister *GetLister(const QModelIndex &idx) const;
  DeviceInfo *GetDevice(const QModelIndex &idx) const;
  SharedPtr<ConnectedDevice> GetConnectedDevice(const QModelIndex &idx) const;
  SharedPtr<ConnectedDevice> GetConnectedDevice(DeviceInfo *device_info) const;

  DeviceInfo *FindDeviceById(const QString &id) const;
  DeviceInfo *FindDeviceByUrl(const QList<QUrl> &url) const;
  QString DeviceNameByID(const QString &unique_id);
  DeviceInfo *FindEquivalentDevice(const QStringList &unique_ids) const;

  // Actions on devices
  SharedPtr<ConnectedDevice> Connect(DeviceInfo *device_info);
  SharedPtr<ConnectedDevice> Connect(const QModelIndex &idx);
  void Disconnect(DeviceInfo *device_info, const QModelIndex &idx);
  void Forget(const QModelIndex &idx);
  void UnmountAsync(const QModelIndex &idx);

  void SetDeviceOptions(const QModelIndex &idx, const QString &friendly_name, const QString &icon_name, const MusicStorage::TranscodeMode mode, const Song::FileType format);

  // QAbstractItemModel
  QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;

 public Q_SLOTS:
  void Unmount(const QModelIndex &idx);

 Q_SIGNALS:
  void ExitFinished();
  void DevicesLoaded(const DeviceDatabaseBackend::DeviceList &devices);
  void DeviceConnected(const QModelIndex idx);
  void DeviceDisconnected(const QModelIndex idx);
  void DeviceCreatedFromDB(DeviceInfo *device_info);
  void DeviceError(const QString &error);

 private Q_SLOTS:
  void PhysicalDeviceAdded(const QString &id);
  void PhysicalDeviceRemoved(const QString &id);
  void PhysicalDeviceChanged(const QString &id);
  void DeviceTaskStarted(const int id);
  void TasksChanged();
  void DeviceSongCountUpdated(const int count);
  void LoadAllDevices();
  void DeviceConnectFinished(const QString &id, bool success);
  void DeviceCloseFinished(const QString &id);
  void AddDevicesFromDB(const DeviceDatabaseBackend::DeviceList &devices);
  void BackendClosed();
  void ListerClosed();
  void DeviceDestroyed();

 private:
  void AddLister(DeviceLister *lister);
  template<typename T> void AddDeviceClass();

  DeviceDatabaseBackend::Device InfoToDatabaseDevice(const DeviceInfo &info) const;

  void RemoveFromDB(DeviceInfo *device_info, const QModelIndex &idx);

  void CloseDevices();
  void CloseListers();
  void CloseBackend();

 private:
  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<Database> database_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<AlbumCoverLoader> albumcover_loader_;

  ScopedPtr<DeviceDatabaseBackend> backend_;

  DeviceStateFilterModel *connected_devices_model_;

  QIcon not_connected_overlay_;

  QList<DeviceLister*> listers_;
  QList<DeviceInfo*> devices_;

  QMultiMap<QString, QMetaObject> device_classes_;

  // Map of task ID to device index
  QMap<int, QPersistentModelIndex> active_tasks_;

  QThreadPool thread_pool_;

  QList<QObject*> wait_for_exit_;
};

template<typename T>
void DeviceManager::AddDeviceClass() {
  QStringList schemes = T::url_schemes();
  QMetaObject obj = T::staticMetaObject;

  for (const QString &scheme : schemes) {
    device_classes_.insert(scheme, obj);
  }
}

#endif  // DEVICEMANAGER_H
