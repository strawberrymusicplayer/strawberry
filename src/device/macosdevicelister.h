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

#ifndef MACDEVICELISTER_H
#define MACDEVICELISTER_H

#include "config.h"

#include <DiskArbitration/DADisk.h>
#include <DiskArbitration/DADissenter.h>
#include <IOKit/IOKitLib.h>

#include <QtGlobal>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QUrl>

#include "devicelister.h"

class MacOsDeviceLister : public DeviceLister {
  Q_OBJECT

 public:
  explicit MacOsDeviceLister();
  ~MacOsDeviceLister();

  QStringList DeviceUniqueIDs();
  QVariantList DeviceIcons(const QString &id);
  QString DeviceManufacturer(const QString &id);
  QString DeviceModel(const QString &id);
  quint64 DeviceCapacity(const QString &id);
  quint64 DeviceFreeSpace(const QString &id);
  QVariantMap DeviceHardwareInfo(const QString &id);
  bool AskForScan(const QString &serial) const;
  QString MakeFriendlyName(const QString &id);
  QList<QUrl> MakeDeviceUrls(const QString &id);

  void UpdateDeviceFreeSpace(const QString &id);

  struct MTPDevice {
    MTPDevice() : capacity(0), free_space(0) {}
    QString vendor;
    QString product;
    quint16 vendor_id;
    quint16 product_id;

    int quirks;
    int bus;
    int address;

    quint64 capacity;
    quint64 free_space;
  };

  void ExitAsync();

 public slots:
  void UnmountDevice(const QString &id);
  void ShutDown();

 private:
  bool Init();

  static void DiskAddedCallback(DADiskRef disk, void* context);
  static void DiskRemovedCallback(DADiskRef disk, void* context);
  static void USBDeviceAddedCallback(void* refcon, io_iterator_t it);
  static void USBDeviceRemovedCallback(void* refcon, io_iterator_t it);

  static void DiskUnmountCallback(DADiskRef disk, DADissenterRef dissenter, void* context);

  void FoundMTPDevice(const MTPDevice& device, const QString& serial);
  void RemovedMTPDevice(const QString& serial);

  quint64 GetFreeSpace(const QUrl& url);
  quint64 GetCapacity(const QUrl& url);

  bool IsCDDevice(const QString& serial) const;

  DASessionRef loop_session_;
  CFRunLoopRef run_loop_;

  QMap<QString, QString> current_devices_;
  QMap<QString, MTPDevice> mtp_devices_;
  QSet<QString> cd_devices_;

  QMutex libmtp_mutex_;

  static QSet<MTPDevice> sMTPDeviceList;
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const MacOsDeviceLister::MTPDevice& device);
#else
uint qHash(const MacOsDeviceLister::MTPDevice& device);
#endif
inline bool operator==(const MacOsDeviceLister::MTPDevice& a, const MacOsDeviceLister::MTPDevice& b) {
  return (a.vendor_id == b.vendor_id) && (a.product_id == b.product_id);
}

#endif
