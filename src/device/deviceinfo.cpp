/*
 * Strawberry Music Player
 * This code was part of Clementine.
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

#include "config.h"

#include <QVariant>
#include <QVariantList>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QPixmap>

#include "core/iconloader.h"

#include "devicelister.h"
#include "devicedatabasebackend.h"
#include "deviceinfo.h"

using namespace Qt::Literals::StringLiterals;

void DeviceInfo::InitFromDb(const DeviceDatabaseBackend::Device &device) {

  database_id_ = device.id_;
  friendly_name_ = device.friendly_name_;
  size_ = device.size_;
  transcode_mode_ = device.transcode_mode_;
  transcode_format_ = device.transcode_format_;
  icon_name_ = device.icon_name_;

  InitIcon();

  const QStringList unique_ids = device.unique_id_.split(u',');
  for (const QString &id : unique_ids) {
    backends_ << Backend(nullptr, id);
  }

}

DeviceDatabaseBackend::Device DeviceInfo::SaveToDb() const {

  DeviceDatabaseBackend::Device device;
  device.friendly_name_ = friendly_name_;
  device.size_ = size_;
  device.id_ = database_id_;
  device.icon_name_ = icon_name_;
  device.transcode_mode_ = transcode_mode_;
  device.transcode_format_ = transcode_format_;

  QStringList unique_ids;
  unique_ids.reserve(backends_.count());
  for (const Backend &backend : backends_) {
    unique_ids << backend.unique_id_;
  }
  device.unique_id_ = unique_ids.join(u',');

  return device;

}

const DeviceInfo::Backend *DeviceInfo::BestBackend() const {

  int best_priority = -1;
  const Backend *backend = nullptr;

  for (int i = 0; i < backends_.count(); ++i) {
    if (backends_[i].lister_ && backends_[i].lister_->priority() > best_priority) {
      best_priority = backends_[i].lister_->priority();
      backend = &(backends_[i]);
    }
  }

  if (!backend && !backends_.isEmpty()) return &(backends_[0]);
  return backend;

}

void DeviceInfo::InitIcon() {

  const QStringList icon_name_list = icon_name_.split(u',');
  QVariantList icons;
  icons.reserve(icon_name_list.count());
  for (const QString &icon_name : icon_name_list) {
    icons << icon_name;
  }
  LoadIcon(icons, friendly_name_);

}

void DeviceInfo::LoadIcon(const QVariantList &icons, const QString &name_hint) {

  icon_name_ = "device"_L1;

  if (icons.isEmpty()) {
    icon_ = IconLoader::Load(icon_name_);
    return;
  }

  // Try to load the icon with that exact name first
  for (const QVariant &icon : icons) {
    if (icon.isNull()) continue;
    if (icon.userType() == QMetaType::QString) {
      QString icon_name = icon.toString();
      if (!icon_name.isEmpty()) {
        icon_ = IconLoader::Load(icon_name);
        if (!icon_.isNull()) {
          icon_name_ = icon_name;
          return;
        }
      }
    }
    else if (!icon.value<QPixmap>().isNull()) {
      icon_ = QIcon(icon.value<QPixmap>());
      return;
    }
  }

  for (const QVariant &icon : icons) {
    if (!icon.isNull() && icon.userType() == QMetaType::QString) {
      QString icon_name = icon.toString();
      if (!icon_name.isEmpty()) {
        QString hint = icons.first().toString().toLower() + name_hint.toLower();
        if (hint.contains("phone"_L1)) icon_name_ = "device-phone"_L1;
        else if (hint.contains("ipod"_L1) || hint.contains("apple"_L1)) icon_name_ = "device-ipod"_L1;
        else if ((hint.contains("usb"_L1)) && (hint.contains("reader"_L1))) icon_name_ = "device-usb-flash"_L1;
        else if (hint.contains("usb"_L1)) icon_name_ = "device-usb-drive"_L1;
        icon_ = IconLoader::Load(icon_name_);
        if (!icon_.isNull()) {
          return;
        }
      }
    }
  }

  icon_name_ = "device"_L1;
  icon_ = IconLoader::Load(icon_name_);

}
