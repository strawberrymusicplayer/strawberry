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

#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "config.h"

#include <memory>

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

#include "core/song.h"
#include "core/musicstorage.h"
#include "core/simpletreemodel.h"
#include "collection/collectionmodel.h"
#include "devicedatabasebackend.h"
#include "deviceinfo.h"

class QModelIndex;
class QPersistentModelIndex;

class Application;
class ConnectedDevice;
class DeviceLister;
class DeviceStateFilterModel;

class DeviceManager : public SimpleTreeModel<DeviceInfo> {
  Q_OBJECT

 public:
  explicit DeviceManager(Application *app, QObject *parent = nullptr);
  ~DeviceManager();

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

  enum State {
    State_Remembered,
    State_NotMounted,
    State_NotConnected,
    State_Connected,
  };

  static const int kDeviceIconSize;
  static const int kDeviceIconOverlaySize;

  void Exit();

  DeviceStateFilterModel *connected_devices_model() const { return connected_devices_model_; }

  // Get info about devices
  int GetDatabaseId(QModelIndex idx) const;
  DeviceLister *GetLister(QModelIndex idx) const;
  DeviceInfo *GetDevice(QModelIndex idx) const;
  std::shared_ptr<ConnectedDevice> GetConnectedDevice(QModelIndex idx) const;
  std::shared_ptr<ConnectedDevice> GetConnectedDevice(DeviceInfo *info) const;

  DeviceInfo *FindDeviceById(const QString &id) const;
  DeviceInfo *FindDeviceByUrl(const QList<QUrl> &url) const;
  QString DeviceNameByID(const QString &unique_id);
  DeviceInfo *FindEquivalentDevice(DeviceInfo *info) const;

  // Actions on devices
  std::shared_ptr<ConnectedDevice> Connect(DeviceInfo *info);
  std::shared_ptr<ConnectedDevice> Connect(QModelIndex idx);
  void Disconnect(DeviceInfo *info, QModelIndex idx);
  void Forget(QModelIndex idx);
  void UnmountAsync(QModelIndex idx);

  void SetDeviceOptions(QModelIndex idx, const QString &friendly_name, const QString &icon_name, MusicStorage::TranscodeMode mode, Song::FileType format);

  // QAbstractItemModel
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

 public slots:
  void Unmount(QModelIndex idx);

 signals:
  void ExitFinished();
  void DeviceConnected(QModelIndex idx);
  void DeviceDisconnected(QModelIndex idx);
  void DeviceCreatedFromDB(DeviceInfo* info);

 private slots:
  void PhysicalDeviceAdded(const QString &id);
  void PhysicalDeviceRemoved(const QString &id);
  void PhysicalDeviceChanged(const QString &id);
  void DeviceTaskStarted(int id);
  void TasksChanged();
  void DeviceSongCountUpdated(int count);
  void LoadAllDevices();
  void DeviceConnectFinished(const QString &id, bool success);
  void DeviceCloseFinished(const QString &id);
  void AddDeviceFromDB(DeviceInfo *info);
  void BackendClosed();
  void ListerClosed();
  void DeviceDestroyed();

 protected:
  void LazyPopulate(DeviceInfo *item) { LazyPopulate(item, true); }
  void LazyPopulate(DeviceInfo *item, bool signal);

 private:

  void AddLister(DeviceLister *lister);
  template <typename T> void AddDeviceClass();

  DeviceDatabaseBackend::Device InfoToDatabaseDevice(const DeviceInfo &info) const;

  void RemoveFromDB(DeviceInfo *info, QModelIndex idx);

  void CloseDevices();
  void CloseListers();
  void CloseBackend();

 private:
  Application *app_;
  DeviceDatabaseBackend *backend_;

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

template <typename T>
void DeviceManager::AddDeviceClass() {
  QStringList schemes = T::url_schemes();
  QMetaObject obj = T::staticMetaObject;

  for (const QString &scheme : schemes) {
    device_classes_.insert(scheme, obj);
  }
}

#endif  // DEVICEMANAGER_H
