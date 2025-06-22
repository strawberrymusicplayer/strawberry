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

#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QMetaType>
#include <QList>
#include <QVariantList>
#include <QString>
#include <QIcon>

#include "core/song.h"
#include "core/musicstorage.h"
#include "core/simpletreemodel.h"
#include "core/simpletreeitem.h"
#include "devicedatabasebackend.h"

class DeviceLister;
class ConnectedDevice;

// Devices can be in three different states:
//  1) Remembered in the database but not physically connected at the moment.
//     database_id valid, lister null, device null
//  2) Physically connected but the user hasn't "connected" it to Strawberry
//     yet.
//     database_id == -1, lister valid, device null
//  3) Physically connected and connected to Strawberry
//     database_id valid, lister valid, device valid
// Devices in all states will have a unique_id.

class DeviceInfo : public SimpleTreeItem<DeviceInfo> {

 public:
  enum class Type {
    Root,
    Device,
  };

  explicit DeviceInfo(SimpleTreeModel<DeviceInfo> *_model)
      : SimpleTreeItem<DeviceInfo>(_model),
        type_(Type::Root),
        database_id_(-1),
        size_(0),
        transcode_mode_(MusicStorage::TranscodeMode::Transcode_Unsupported),
        transcode_format_(Song::FileType::Unknown),
        task_percentage_(-1),
        unmount_(false),
        forget_(false) {}

  explicit DeviceInfo(const Type _type, DeviceInfo *_parent = nullptr)
      : SimpleTreeItem<DeviceInfo>(_parent),
        type_(_type),
        database_id_(-1),
        size_(0),
        transcode_mode_(MusicStorage::TranscodeMode::Transcode_Unsupported),
        transcode_format_(Song::FileType::Unknown),
        task_percentage_(-1),
        unmount_(false),
        forget_(false) {}

  // A device can be discovered in different ways (udisks2, gio, etc.)
  // Sometimes the same device is discovered more than once.  In this case the device will have multiple "backends".
  struct Backend {
    explicit Backend(DeviceLister *lister = nullptr, const QString &id = QString())
        : lister_(lister),
          unique_id_(id) {}

    DeviceLister *lister_;  // nullptr if not physically connected
    QString unique_id_;
  };

  // Serialising to the database
  void InitFromDb(const DeviceDatabaseBackend::Device &dev);
  DeviceDatabaseBackend::Device SaveToDb() const;

  void InitIcon();
  // Tries to load a good icon for the device.  Sets icon_name_ and icon_.
  void LoadIcon(const QVariantList &icons, const QString &name_hint);

  // Gets the best backend available (the one with the highest priority)
  const Backend *BestBackend() const;

  Type type_;
  int database_id_;  // -1 if not remembered in the database
  SharedPtr<ConnectedDevice> device_;  // nullptr if not connected
  QList<Backend> backends_;

  QString friendly_name_;
  quint64 size_;

  QString icon_name_;
  QIcon icon_;

  MusicStorage::TranscodeMode transcode_mode_;
  Song::FileType transcode_format_;

  int task_percentage_;

  bool unmount_;
  bool forget_;

  Q_DISABLE_COPY(DeviceInfo)
};

Q_DECLARE_METATYPE(DeviceInfo::Type)

#endif  // DEVICEINFO_H
