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

#include <memory>

#include <QObject>
#include <QAbstractItemModel>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/database.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectiondirectory.h"
#include "connecteddevice.h"
#include "devicelister.h"
#include "devicemanager.h"
#include "deviceinfo.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

ConnectedDevice::ConnectedDevice(const QUrl &url,
                                 DeviceLister *lister,
                                 const QString &unique_id,
                                 DeviceManager *device_manager,
                                 const SharedPtr<TaskManager> task_manager,
                                 const SharedPtr<Database> database,
                                 const SharedPtr<TagReaderClient> tagreader_client,
                                 const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                 const int database_id,
                                 const bool first_time,
                                 QObject *parent)
    : QObject(parent),
      url_(url),
      first_time_(first_time),
      lister_(lister),
      unique_id_(unique_id),
      database_id_(database_id),
      device_manager_(device_manager),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      song_count_(0) {

  Q_UNUSED(tagreader_client)

  qLog(Info) << "Connected" << url << unique_id << first_time;

  // Create the backend in the database thread.
  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->moveToThread(database->thread());
  qLog(Debug) << &*collection_backend_ << "for device" << unique_id_ << "moved to thread" << database->thread();

  if (url_.scheme() != "cdda"_L1) {
    QObject::connect(&*collection_backend_, &CollectionBackend::TotalSongCountUpdated, this, &ConnectedDevice::BackendTotalSongCountUpdated);
  }

  collection_backend_->Init(database,
                            task_manager,
                            Song::Source::Device,
                            QStringLiteral("device_%1_songs").arg(database_id),
                            QStringLiteral("device_%1_directories").arg(database_id),
                            QStringLiteral("device_%1_subdirectories").arg(database_id));

  // Create the model
  collection_model_ = new CollectionModel(collection_backend_, albumcover_loader, this);

}

void ConnectedDevice::InitBackendDirectory(const QString &mount_point, const bool first_time, const bool rewrite_path) {

  QList<CollectionDirectory> directories = collection_backend_->GetAllDirectories();
  if (first_time || directories.isEmpty()) {
    collection_backend_->AddDirectory(mount_point);
  }
  else {
    if (rewrite_path) {
      // This is a bit of a hack.
      // The device might not be mounted at the same path each time,
      // so if it's different we have to munge all the paths in the database to fix it.
      // This can be done entirely in sqlite so it's relatively fast...

      // Get the directory it was mounted at last time.  Devices only have one directory (the root).
      CollectionDirectory dir = directories[0];
      if (dir.path != mount_point) {
        // The directory is different, commence the munging.
        qLog(Info) << "Changing path from" << dir.path << "to" << mount_point;
        collection_backend_->ChangeDirPath(dir.id, dir.path, mount_point);
      }
    }

    // Load the directory properly now
    collection_backend_->LoadDirectoriesAsync();
  }

}

void ConnectedDevice::ConnectAsync() { Q_EMIT DeviceConnectFinished(unique_id_, true); }

void ConnectedDevice::Close() {

  QObject::connect(&*collection_backend_, &CollectionBackend::ExitFinished, this, &ConnectedDevice::BackendCloseFinished);
  collection_backend_->ExitAsync();

}

void ConnectedDevice::BackendCloseFinished() {

  Q_EMIT DeviceCloseFinished(unique_id_);

}

void ConnectedDevice::Eject() {

  DeviceInfo *info = device_manager_->FindDeviceById(unique_id_);
  if (!info) return;

  QModelIndex idx = device_manager_->ItemToIndex(info);
  if (!idx.isValid()) return;

  device_manager_->UnmountAsync(idx);

}

bool ConnectedDevice::FinishCopy(bool success, QString &error_text) {

  Q_UNUSED(error_text)

  lister_->UpdateDeviceFreeSpace(unique_id_);

  return success;

}

bool ConnectedDevice::FinishDelete(bool success, QString &error_text) {
  Q_UNUSED(error_text)
  lister_->UpdateDeviceFreeSpace(unique_id_);
  return success;
}

MusicStorage::TranscodeMode ConnectedDevice::GetTranscodeMode() const {

  DeviceInfo *info = device_manager_->FindDeviceById(unique_id_);
  if (!info) return MusicStorage::TranscodeMode();

  QModelIndex idx = device_manager_->ItemToIndex(info);
  if (!idx.isValid()) return MusicStorage::TranscodeMode();

  return static_cast<MusicStorage::TranscodeMode>(idx.data(DeviceManager::Role_TranscodeMode).toInt());

}

Song::FileType ConnectedDevice::GetTranscodeFormat() const {

  DeviceInfo *info = device_manager_->FindDeviceById(unique_id_);
  if (!info) return Song::FileType::Unknown;

  QModelIndex idx = device_manager_->ItemToIndex(info);
  if (!idx.isValid()) return Song::FileType::Unknown;

  return static_cast<Song::FileType>(idx.data(DeviceManager::Role_TranscodeFormat).toInt());

}

void ConnectedDevice::BackendTotalSongCountUpdated(int count) {
  song_count_ = count;
  Q_EMIT SongCountUpdated(count);
}
