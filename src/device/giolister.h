/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef GIOLISTER_H
#define GIOLISTER_H

#include "config.h"

// Work around compile issue with glib >= 2.25
#ifdef signals
#  undef signals
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/scopedgobject.h"
#include "devicelister.h"

class GioLister : public DeviceLister {
  Q_OBJECT

 public:
  explicit GioLister() {}
  ~GioLister() override;

  int priority() const override { return 50; }

  QStringList DeviceUniqueIDs() override;
  QVariantList DeviceIcons(const QString &id) override;
  QString DeviceManufacturer(const QString &id) override;
  QString DeviceModel(const QString &id) override;
  quint64 DeviceCapacity(const QString &id) override;
  quint64 DeviceFreeSpace(const QString &id) override;
  QVariantMap DeviceHardwareInfo(const QString &id) override;
  bool DeviceNeedsMount(const QString &id) override;

  QString MakeFriendlyName(const QString &id) override;
  QList<QUrl> MakeDeviceUrls(const QString &id) override;

 public slots:
  void MountDevice(const QString &id, const int request_id) override;
  void UnmountDevice(const QString &id) override;
  void UpdateDeviceFreeSpace(const QString &id) override;

 protected:
  bool Init() override;

 private:
  struct DeviceInfo {
    DeviceInfo() : drive_removable(false), filesystem_size(0),filesystem_free(0), invalid_enclosing_mount(false) {}

    QString unique_id() const;
    bool is_suitable() const;

    static QString ConvertAndFree(char *str);
    void ReadDriveInfo(GDrive *drive);
    void ReadVolumeInfo(GVolume *volume);
    void ReadMountInfo(GMount *mount);

    // Only available if it's a physical drive
    ScopedGObject<GVolume> volume_ptr;
    QString volume_name;
    QString volume_unix_device;
    QString volume_root_uri;
    QString volume_uuid;

    // Only available if it's a physical drive
    ScopedGObject<GDrive> drive_ptr;
    QString drive_name;
    bool drive_removable;

    // Only available if it's mounted
    ScopedGObject<GMount> mount_ptr;
    QString mount_path;
    QString mount_uri;
    QString mount_name;
    QStringList mount_icon_names;
    QString mount_uuid;
    quint64 filesystem_size;
    quint64 filesystem_free;
    QString filesystem_type;

    bool invalid_enclosing_mount;
  };

  void VolumeAdded(GVolume *volume);
  void VolumeRemoved(GVolume *volume);

  void MountAdded(GMount *mount);
  void MountChanged(GMount *mount);
  void MountRemoved(GMount *mount);

  static void VolumeAddedCallback(GVolumeMonitor*, GVolume*, gpointer);
  static void VolumeRemovedCallback(GVolumeMonitor*, GVolume*, gpointer);

  static void MountAddedCallback(GVolumeMonitor*, GMount*, gpointer);
  static void MountChangedCallback(GVolumeMonitor*, GMount*, gpointer);
  static void MountRemovedCallback(GVolumeMonitor*, GMount*, gpointer);

  static void VolumeMountFinished(GObject *object, GAsyncResult *result, gpointer);
  static void VolumeEjectFinished(GObject *object, GAsyncResult *result, gpointer);
  static void MountEjectFinished(GObject *object, GAsyncResult *result, gpointer);
  static void MountUnmountFinished(GObject *object, GAsyncResult *result, gpointer);

  // You MUST hold the mutex while calling this function
  QString FindUniqueIdByMount(GMount *mount) const;
  QString FindUniqueIdByVolume(GVolume *volume) const;

  template <typename T>
  T LockAndGetDeviceInfo(const QString &id, T DeviceInfo::*field);

 private:
  ScopedGObject<GVolumeMonitor> monitor_;
  QList<gulong> signals_;

  QMutex mutex_;
  QMap<QString, DeviceInfo> devices_;
};

template <typename T>
T GioLister::LockAndGetDeviceInfo(const QString &id, T DeviceInfo::*field) {
  QMutexLocker l(&mutex_);
  if (!devices_.contains(id)) return T();

  return devices_[id].*field;
}

#endif  // GIOLISTER_H
