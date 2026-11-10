/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <atomic>

#include <QtGlobal>
#include <QObject>
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
  explicit MacOsDeviceLister(QObject *parent = nullptr);
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

#ifdef HAVE_MTP
  struct MTPDevice {
    MTPDevice() : vendor_id(0), product_id(0), quirks(0), bus(0), address(0), capacity(0), free_space(0) {}
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
#endif  // HAVE_MTP

  void ExitAsync();
  void UnmountDeviceAsync(const QString &id);

 public Q_SLOTS:
  void UnmountDevice(const QString &id);
  void ShutDown();

 private:
  bool Init();

  static void DiskAddedCallback(DADiskRef disk, void *context);
  static void DiskRemovedCallback(DADiskRef disk, void *context);
  static void USBDeviceAddedCallback(void *refcon, io_iterator_t it);
  static void USBDeviceRemovedCallback(void *refcon, io_iterator_t it);

  static void DiskUnmountCallback(DADiskRef disk, DADissenterRef dissenter, void *context);

#ifdef HAVE_MTP
  void FoundMTPDevice(const MTPDevice &mtp_device, const QString &serial);
  void RemovedMTPDevice(const QString &serial);
  quint64 GetFreeSpace(const QUrl &url);
  quint64 GetCapacity(const QUrl &url);
#endif  // HAVE_MTP

  bool IsCDDevice(const QString &serial) const;
  QString BSDName(const QString &serial) const;
#ifdef HAVE_MTP
  MTPDevice MTPDeviceForSerial(const QString &serial) const;
#endif

  DASessionRef loop_session_ = nullptr;
  // The run loop of this object's thread, retained, it's set by Init() and stopped by ShutDown() from another thread.
  std::atomic<CFRunLoopRef> run_loop_ = nullptr;
  // Set by ShutDown(), so Init() does not run the run loop when ShutDown() was called before the run loop was set.
  std::atomic<bool> shutdown_requested_ = false;

  // The devices are added and removed by the DiskArbitration and IOKit callbacks on this object's thread, and read by DeviceManager on the GUI thread.
  // devices_mutex_ protects them, it's never held while emitting signals, calling into DiskArbitration, IOKit or libmtp, or locking libmtp_mutex_.
  mutable QMutex devices_mutex_;
  QMap<QString, QString> current_devices_;
#ifdef HAVE_MTP
  QMap<QString, MTPDevice> mtp_devices_;
#endif
  QSet<QString> cd_devices_;

#ifdef HAVE_MTP
  QMutex libmtp_mutex_;
  static QSet<MTPDevice> sMTPDeviceList;
#endif
};

#ifdef HAVE_MTP
size_t qHash(const MacOsDeviceLister::MTPDevice &mtp_device);

inline bool operator==(const MacOsDeviceLister::MTPDevice &a, const MacOsDeviceLister::MTPDevice &b) {
  return (a.vendor_id == b.vendor_id) && (a.product_id == b.product_id);
}
#endif  // HAVE_MTP

#endif  // MACDEVICELISTER_H
