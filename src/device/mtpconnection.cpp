/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <cstdlib>
#include <cstdint>

#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

#include "core/logging.h"
#include "mtpconnection.h"

using namespace Qt::Literals::StringLiterals;

MtpConnection::MtpConnection(const QUrl &url, QObject *parent) : QObject(parent), device_(nullptr) {

  QString hostname = url.host();
  // Parse the URL
  static const QRegularExpression host_re(u"^usb-(\\d+)-(\\d+)$"_s);

  unsigned int bus_location = 0;
  unsigned int device_num = 0;

  QUrlQuery url_query(url);

  QRegularExpressionMatch re_match = host_re.match(hostname);
  if (re_match.hasMatch()) {
    bus_location = re_match.captured(1).toUInt();
    device_num = re_match.captured(2).toUInt();
  }
  else if (url_query.hasQueryItem(u"busnum"_s)) {
    bus_location = url_query.queryItemValue(u"busnum"_s).toUInt();
    device_num = url_query.queryItemValue(u"devnum"_s).toUInt();
  }
  else {
    error_text_ = tr("Invalid MTP device: %1").arg(hostname);
    qLog(Error) << error_text_;
    return;
  }

  if (url_query.hasQueryItem(u"vendor"_s)) {
    LIBMTP_raw_device_t *raw_device = static_cast<LIBMTP_raw_device_t*>(malloc(sizeof(LIBMTP_raw_device_t)));
    raw_device->device_entry.vendor = url_query.queryItemValue(u"vendor"_s).toLatin1().data();
    raw_device->device_entry.product = url_query.queryItemValue(u"product"_s).toLatin1().data();
    raw_device->device_entry.vendor_id = url_query.queryItemValue(u"vendor_id"_s).toUShort();
    raw_device->device_entry.product_id = url_query.queryItemValue(u"product_id"_s).toUShort();
    raw_device->device_entry.device_flags = url_query.queryItemValue(u"quirks"_s).toUInt();

    raw_device->bus_location = bus_location;
    raw_device->devnum = device_num;

    device_ = LIBMTP_Open_Raw_Device(raw_device);  // NOLINT(clang-analyzer-unix.Malloc)
    if (!device_) {
      error_text_ = tr("Could not open MTP device.");
      qLog(Error) << error_text_;
    }
    return;
  }

  // Get a list of devices from libmtp and figure out which one is ours
  int count = 0;
  LIBMTP_raw_device_t *raw_devices = nullptr;
  LIBMTP_error_number_t error_number = LIBMTP_Detect_Raw_Devices(&raw_devices, &count);
  if (error_number != LIBMTP_ERROR_NONE) {
    error_text_ = tr("MTP error: %1").arg(ErrorString(error_number));
    qLog(Error) << error_text_;
    return;
  }

  LIBMTP_raw_device_t *raw_device = nullptr;
  for (int i = 0; i < count; ++i) {
    if (raw_devices[i].bus_location == bus_location && raw_devices[i].devnum == device_num) {
      raw_device = &raw_devices[i];
      break;
    }
  }

  if (!raw_device) {
    error_text_ = tr("MTP device not found.");
    qLog(Error) << error_text_;
    free(raw_devices);
    return;
  }

  // Connect to the device
  device_ = LIBMTP_Open_Raw_Device(raw_device);
  if (!device_) {
    error_text_ = tr("Could not open MTP device.");
    qLog(Error) << error_text_;
  }

  free(raw_devices);

}

MtpConnection::~MtpConnection() {
  if (device_) LIBMTP_Release_Device(device_);
}

QString MtpConnection::ErrorString(const LIBMTP_error_number_t error_number) {

  switch(error_number) {
    case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
      return u"No Devices have been found."_s;
    case LIBMTP_ERROR_CONNECTING:
      return u"There has been an error connecting."_s;
    case LIBMTP_ERROR_MEMORY_ALLOCATION:
      return u"Memory Allocation Error."_s;
    case LIBMTP_ERROR_GENERAL:
    default:
      return u"Unknown error, please report this to the libmtp developers."_s;
    case LIBMTP_ERROR_NONE:
      return u"Successfully connected."_s;
  }

}

bool MtpConnection::GetSupportedFiletypes(QList<Song::FileType> *ret) {

  if (!device_) return false;

  uint16_t *list = nullptr;
  uint16_t length = 0;

  if (LIBMTP_Get_Supported_Filetypes(device_, &list, &length) != 0 || !list || !length) {
    return false;
  }

  for (int i = 0; i < length; ++i) {
    switch (static_cast<LIBMTP_filetype_t>(list[i])) {
      case LIBMTP_FILETYPE_WAV:  *ret << Song::FileType::WAV; break;
      case LIBMTP_FILETYPE_MP2:
      case LIBMTP_FILETYPE_MP3:  *ret << Song::FileType::MPEG; break;
      case LIBMTP_FILETYPE_WMA:  *ret << Song::FileType::ASF; break;
      case LIBMTP_FILETYPE_MP4:
      case LIBMTP_FILETYPE_M4A:
      case LIBMTP_FILETYPE_AAC:  *ret << Song::FileType::MP4; break;
      case LIBMTP_FILETYPE_FLAC:
        *ret << Song::FileType::FLAC;
        *ret << Song::FileType::OggFlac;
        break;
      case LIBMTP_FILETYPE_OGG:
        *ret << Song::FileType::OggVorbis;
        *ret << Song::FileType::OggSpeex;
        *ret << Song::FileType::OggFlac;
        break;
      default:
        qLog(Error) << "Unknown MTP file format" << LIBMTP_Get_Filetype_Description(static_cast<LIBMTP_filetype_t>(list[i]));
        break;
    }
  }

  free(list);
  return true;

}
