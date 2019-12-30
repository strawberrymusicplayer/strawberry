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

#ifndef CDDALISTER_H
#define CDDALISTER_H

#include <config.h>


#include <QtGlobal>
#include <QObject>
#include <QMetaType>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "devicelister.h"

class CddaLister : public DeviceLister {
  Q_OBJECT

 public:
  CddaLister() {}

  QStringList DeviceUniqueIDs();
  QVariantList DeviceIcons(const QString &id);
  QString DeviceManufacturer(const QString &id);
  QString DeviceModel(const QString &id);
  quint64 DeviceCapacity(const QString &id);
  quint64 DeviceFreeSpace(const QString &id);
  QVariantMap DeviceHardwareInfo(const QString &id);
  bool AskForScan(const QString&) const { return false; }
  QString MakeFriendlyName(const QString&);
  QList<QUrl> MakeDeviceUrls(const QString&);
  void UnmountDevice(const QString&);
  void UpdateDeviceFreeSpace(const QString&);
  bool Init();

 private:
  QStringList devices_list_;
};
#endif  // CDDALISTER_H
