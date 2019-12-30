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

#ifndef DEVICELISTER_H
#define DEVICELISTER_H

#include "config.h"


#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>

class DeviceLister : public QObject {
  Q_OBJECT

 public:
  DeviceLister();
  virtual ~DeviceLister();

  // Tries to start the thread and initialise the engine.  This object will be moved to the new thread.
  void Start();
  virtual void ExitAsync();

  // If two listers know about the same device, then the metadata will get taken from the one with the highest priority.
  virtual int priority() const { return 100; }

  // Query information about the devices that are available.  Must be thread-safe.
  virtual QStringList DeviceUniqueIDs() = 0;
  virtual QVariantList DeviceIcons(const QString &id) = 0;
  virtual QString DeviceManufacturer(const QString &id) = 0;
  virtual QString DeviceModel(const QString &id) = 0;
  virtual quint64 DeviceCapacity(const QString &id) = 0;
  virtual quint64 DeviceFreeSpace(const QString &id) = 0;
  virtual QVariantMap DeviceHardwareInfo(const QString &id) = 0;
  virtual bool DeviceNeedsMount(const QString &id) { Q_UNUSED(id); return false; }

  // When connecting to a device for the first time, do we want an user's confirmation for scanning it? (by default yes)
  virtual bool AskForScan(const QString &id) const { Q_UNUSED(id); return true; }

  virtual QString MakeFriendlyName(const QString &id) = 0;
  virtual QList<QUrl> MakeDeviceUrls(const QString &id) = 0;

  // Ensure the device is mounted.  This should run asynchronously and emit DeviceMounted when it's done.
  virtual int MountDeviceAsync(const QString &id);

  // Do whatever needs to be done to safely remove the device.
  virtual void UnmountDeviceAsync(const QString &id);

 public slots:
  virtual void UpdateDeviceFreeSpace(const QString &id) = 0;
  virtual void ShutDown() {}
  virtual void MountDevice(const QString &id, const int ret);
  virtual void UnmountDevice(const QString &id) { Q_UNUSED(id); }
  virtual void Exit();

 signals:
  void DeviceAdded(const QString &id);
  void DeviceRemoved(const QString &id);
  void DeviceChanged(const QString &id);
  void DeviceMounted(const QString &id, int request_id, bool success);
  void ExitFinished();

 protected:
  virtual bool Init() = 0;
  QUrl MakeUrlFromLocalPath(const QString &path) const;
  bool IsIpod(const QString &path) const;

  QStringList GuessIconForPath(const QString &path);
  QStringList GuessIconForModel(const QString &vendor, const QString &model);

 protected:
  QThread *thread_;
  QThread *original_thread_;
  int next_mount_request_id_;

 private slots:
  void ThreadStarted();
};

#endif  // DEVICELISTER_H

