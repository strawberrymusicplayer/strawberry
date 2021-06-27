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

#include <QObject>
#include <QThread>
#include <QAbstractItemModel>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "core/logging.h"
#include "core/application.h"
#include "core/database.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/directory.h"
#include "connecteddevice.h"
#include "devicelister.h"
#include "devicemanager.h"
#include "deviceinfo.h"

ConnectedDevice::ConnectedDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, DeviceManager *manager, Application *app, const int database_id, const bool first_time, QObject *parent)
    : QObject(parent),
      app_(app),
      url_(url),
      first_time_(first_time),
      lister_(lister),
      unique_id_(unique_id),
      database_id_(database_id),
      manager_(manager),
      backend_(nullptr),
      model_(nullptr),
      song_count_(0) {

  qLog(Info) << "Connected" << url << unique_id << first_time;

  // Create the backend in the database thread.
  backend_ = new CollectionBackend();
  backend_->moveToThread(app_->database()->thread());
  qLog(Debug) << backend_ << "for device" << unique_id_ << "moved to thread" << app_->database()->thread();

  if (url_.scheme() != "cdda") {
    QObject::connect(backend_, &CollectionBackend::TotalSongCountUpdated, this, &ConnectedDevice::BackendTotalSongCountUpdated);
  }

  backend_->Init(app_->database(),
                 Song::Source_Device,
                 QString("device_%1_songs").arg(database_id),
                 QString("device_%1_directories").arg(database_id),
                 QString("device_%1_subdirectories").arg(database_id));

  // Create the model
  model_ = new CollectionModel(backend_, app_, this);

}

ConnectedDevice::~ConnectedDevice() {
  backend_->deleteLater();
}

void ConnectedDevice::InitBackendDirectory(const QString &mount_point, const bool first_time, const bool rewrite_path) {

  QList<Directory> directories = backend_->GetAllDirectories();
  if (first_time || directories.isEmpty()) {
    backend_->AddDirectory(mount_point);
  }
  else {
    if (rewrite_path) {
      // This is a bit of a hack.
      // The device might not be mounted at the same path each time,
      // so if it's different we have to munge all the paths in the database to fix it.
      // This can be done entirely in sqlite so it's relatively fast...

      // Get the directory it was mounted at last time.  Devices only have one directory (the root).
      Directory dir = directories[0];
      if (dir.path != mount_point) {
        // The directory is different, commence the munging.
        qLog(Info) << "Changing path from" << dir.path << "to" << mount_point;
        backend_->ChangeDirPath(dir.id, dir.path, mount_point);
      }
    }

    // Load the directory properly now
    backend_->LoadDirectoriesAsync();
  }

}

void ConnectedDevice::ConnectAsync() { emit DeviceConnectFinished(unique_id_, true); }

void ConnectedDevice::Close() {

  QObject::connect(backend_, &CollectionBackend::ExitFinished, this, &ConnectedDevice::BackendCloseFinished);
  backend_->ExitAsync();

}

void ConnectedDevice::BackendCloseFinished() {

  emit DeviceCloseFinished(unique_id_);

}

void ConnectedDevice::Eject() {

  DeviceInfo *info = manager_->FindDeviceById(unique_id_);
  if (!info) return;

  QModelIndex idx = manager_->ItemToIndex(info);
  if (!idx.isValid()) return;

  manager_->UnmountAsync(idx);

}

void ConnectedDevice::FinishCopy(bool) {
  lister_->UpdateDeviceFreeSpace(unique_id_);
}

void ConnectedDevice::FinishDelete(bool) {
  lister_->UpdateDeviceFreeSpace(unique_id_);
}

MusicStorage::TranscodeMode ConnectedDevice::GetTranscodeMode() const {

  DeviceInfo *info = manager_->FindDeviceById(unique_id_);
  if (!info) return MusicStorage::TranscodeMode();

  QModelIndex idx = manager_->ItemToIndex(info);
  if (!idx.isValid()) return MusicStorage::TranscodeMode();

  return MusicStorage::TranscodeMode(idx.data(DeviceManager::Role_TranscodeMode).toInt());

}

Song::FileType ConnectedDevice::GetTranscodeFormat() const {

  DeviceInfo *info = manager_->FindDeviceById(unique_id_);
  if (!info) return Song::FileType_Unknown;

  QModelIndex idx = manager_->ItemToIndex(info);
  if (!idx.isValid()) return Song::FileType_Unknown;

  return Song::FileType(idx.data(DeviceManager::Role_TranscodeFormat).toInt());

}

void ConnectedDevice::BackendTotalSongCountUpdated(int count) {
  song_count_ = count;
  emit SongCountUpdated(count);
}
