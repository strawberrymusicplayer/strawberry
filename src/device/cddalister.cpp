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

#include <config.h>

#include <cstddef>

#include <cdio/cdio.h>
#include <cdio/device.h>

#include <QtGlobal>
#include <QFileInfo>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>

#include "cddalister.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

QStringList CDDALister::DeviceUniqueIDs() { return devices_list_; }

QVariantList CDDALister::DeviceIcons(const QString &id) {

  Q_UNUSED(id)

  QVariantList icons;
  icons << u"media-optical"_s;
  return icons;

}

QString CDDALister::DeviceManufacturer(const QString &id) {

  CdIo_t *cdio = cdio_open(id.toLocal8Bit().constData(), DRIVER_DEVICE);
  cdio_hwinfo_t cd_info;
  if (cdio_get_hwinfo(cdio, &cd_info)) {
    cdio_destroy(cdio);
    return QString::fromUtf8(cd_info.psz_vendor);
  }
  cdio_destroy(cdio);
  return QString();

}

QString CDDALister::DeviceModel(const QString &id) {

  CdIo_t *cdio = cdio_open(id.toLocal8Bit().constData(), DRIVER_DEVICE);
  cdio_hwinfo_t cd_info;
  if (cdio_get_hwinfo(cdio, &cd_info)) {
    cdio_destroy(cdio);
    return QString::fromUtf8(cd_info.psz_model);
  }
  cdio_destroy(cdio);
  return QString();

}

quint64 CDDALister::DeviceCapacity(const QString &id) {

  Q_UNUSED(id)

  return 0;

}

quint64 CDDALister::DeviceFreeSpace(const QString &id) {

  Q_UNUSED(id)

  return 0;

}

QVariantMap CDDALister::DeviceHardwareInfo(const QString &id) {
  Q_UNUSED(id)
  return QVariantMap();
}

QString CDDALister::MakeFriendlyName(const QString &id) {

  CdIo_t *cdio = cdio_open(id.toLocal8Bit().constData(), DRIVER_DEVICE);
  cdio_hwinfo_t cd_info;
  if (cdio_get_hwinfo(cdio, &cd_info)) {
    const QString friendly_name = QString::fromUtf8(cd_info.psz_model).trimmed();
    cdio_destroy(cdio);
    return friendly_name;
  }
  cdio_destroy(cdio);
  return u"CD ("_s + id + QLatin1Char(')');

}

QList<QUrl> CDDALister::MakeDeviceUrls(const QString &id) {
  return QList<QUrl>() << QUrl(u"cdda://"_s + id);
}

void CDDALister::UnmountDevice(const QString &id) {
  cdio_eject_media_drive(id.toLocal8Bit().constData());
}

void CDDALister::UpdateDeviceFreeSpace(const QString &id) {
  Q_UNUSED(id)
}

bool CDDALister::Init() {

  cdio_init();
#ifdef Q_OS_MACOS
  if (!cdio_have_driver(DRIVER_OSX)) {
    qLog(Error) << "libcdio was compiled without support for macOS!";
  }
#endif
  char **devices = cdio_get_devices(DRIVER_DEVICE);
  if (!devices) {
    qLog(Debug) << "No CD devices found";
    return false;
  }
  for (; *devices != nullptr; ++devices) {
    QString device = QString::fromUtf8(*devices);
    QFileInfo device_info(device);
    if (device_info.isSymLink()) {
      device = device_info.symLinkTarget();
    }
#ifdef Q_OS_MACOS
    // Every track is detected as a separate device on Darwin. The raw disk looks like /dev/rdisk1
    if (!device.contains(QRegularExpression(u"^/dev/rdisk[0-9]$"_s))) {
      continue;
    }
#endif
    if (!devices_list_.contains(device)) {
      devices_list_ << device;
      Q_EMIT DeviceAdded(device);
    }
  }

  return true;

}
