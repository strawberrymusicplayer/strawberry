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

#ifndef ILISTER_H
#define ILISTER_H

#include "config.h"

#include <libimobiledevice/libimobiledevice.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>

#include "devicelister.h"

class iLister : public DeviceLister {
  Q_OBJECT

 public:
  explicit iLister();
  ~iLister() override;

  int priority() const override { return 120; }

  QStringList DeviceUniqueIDs() override;
  QVariantList DeviceIcons(const QString &id) override;
  QString DeviceManufacturer(const QString &id) override;
  QString DeviceModel(const QString &id) override;
  quint64 DeviceCapacity(const QString &id) override;
  quint64 DeviceFreeSpace(const QString &id) override;
  QVariantMap DeviceHardwareInfo(const QString &id) override;
  QString MakeFriendlyName(const QString &id) override;
  QList<QUrl> MakeDeviceUrls(const QString &id) override;

 public slots:
  void UpdateDeviceFreeSpace(const QString &id) override;

 private:
  struct DeviceInfo {
    DeviceInfo() : valid(false), free_bytes(0), total_bytes(0), password_protected(false) {}

    bool valid;

    QString uuid;
    QString product_type;
    quint64 free_bytes;
    quint64 total_bytes;
    QString name;  // Name given to the iDevice by the user.

    // Extra information.
    QString colour;
    QString imei;
    QString hardware;
    bool password_protected;
    QString os_version;
    QString timezone;
    QString wifi_mac;
    QString bt_mac;
  };

  bool Init() override;

  static void EventCallback(const idevice_event_t *event, void *context);

  void DeviceAddedCallback(const QString uuid);
  void DeviceRemovedCallback(const QString uuid);

  DeviceInfo ReadDeviceInfo(const QString uuid);
  static QString UniqueId(const QString uuid);

  template <typename T>
  T LockAndGetDeviceInfo(const QString &id, T DeviceInfo::*field);

 private:
  QMutex mutex_;
  QMap<QString, DeviceInfo> devices_;
};

template <typename T>
T iLister::LockAndGetDeviceInfo(const QString &id, T DeviceInfo::*field) {
  QMutexLocker l(&mutex_);
  if (!devices_.contains(id))
    return T();

  return devices_[id].*field;
}

#endif
