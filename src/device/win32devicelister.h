/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef WIN32DEVICELISTER_H
#define WIN32DEVICELISTER_H

#include "config.h"

#include <atomic>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "devicelister.h"

class QTimer;

// Lists removable mass storage volumes (USB flash drives, USB hard drives, memory cards) that have a drive letter.
class Win32DeviceLister : public DeviceLister {
  Q_OBJECT

 public:
  explicit Win32DeviceLister(QObject *parent = nullptr);
  ~Win32DeviceLister() override;

  QStringList DeviceUniqueIDs() override;
  QVariantList DeviceIcons(const QString &id) override;
  QString DeviceManufacturer(const QString &id) override;
  QString DeviceModel(const QString &id) override;
  quint64 DeviceCapacity(const QString &id) override;
  quint64 DeviceFreeSpace(const QString &id) override;
  QVariantMap DeviceHardwareInfo(const QString &id) override;
  QString MakeFriendlyName(const QString &id) override;
  QList<QUrl> MakeDeviceUrls(const QString &id) override;

 public Q_SLOTS:
  void UnmountDevice(const QString &id) override;
  void UpdateDeviceFreeSpace(const QString &id) override;
  void ShutDown() override;

 protected:
  bool Init() override;

 private Q_SLOTS:
  void ScheduleRescan();
  void RescanTimeout();
  void CheckLogicalDrives();

 private:
  struct DeviceData {
    DeviceData();
    bool IsSameDevice(const DeviceData &other) const;

    QString unique_id;
    QString root_path;    // E:/
    QString volume_name;  // \\?\Volume{GUID}\ (Volume GUID path)
    QString label;
    QString filesystem;
    quint32 volume_serial;
    QString vendor;
    QString product;
    QString serial;
    bool removable_drive;
    quint64 capacity;
    quint64 free_space;
    quint64 free_space_updates;  // Incremented by UpdateDeviceFreeSpace(), so a rescan can tell whether it was updated while scanning.
  };

  enum class ReadResult {
    Unsuitable,  // Not a removable volume.
    UnknownBus,  // A fixed drive where the bus type couldn't be read, so it's unknown whether it's a removable drive, such as a USB hard drive.
    NotReady,    // A removable drive, but the volume can't be read, such as an empty card reader or a locked BitLocker volume.
    Ready
  };

  static ReadResult ReadDeviceData(const QChar drive_letter, DeviceData &device_data);
  static bool EjectDrive(const QString &volume_name, const QString &root_path);
  void Rescan();
  void UnregisterNotification();

  QReadWriteLock device_data_lock_;
  QMap<QString, DeviceData> device_data_;

  std::atomic<void*> notification_;
  std::atomic<bool> shutdown_;
  QTimer *timer_rescan_;
  QTimer *timer_logical_drives_;
  int rescans_pending_;
  quint32 logical_drives_;
  quint32 not_ready_drives_;
  quint32 unknown_bus_drives_;
  int unknown_bus_retries_;
};

#endif  // WIN32DEVICELISTER_H
