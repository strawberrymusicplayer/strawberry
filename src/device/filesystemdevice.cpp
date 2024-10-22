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

#include "config.h"

#include <QThread>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/database.h"

#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionwatcher.h"

#include "covermanager/albumcoverloader.h"

#include "connecteddevice.h"
#include "devicemanager.h"
#include "filesystemdevice.h"

class DeviceLister;

FilesystemDevice::FilesystemDevice(const QUrl &url,
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
    : FilesystemMusicStorage(Song::Source::Device, url.toLocalFile()),
      ConnectedDevice(url, lister, unique_id, device_manager, task_manager, database, tagreader_client, albumcover_loader, database_id, first_time, parent),
      collection_watcher_(new CollectionWatcher(Song::Source::Device, task_manager, tagreader_client, collection_backend_)),
      watcher_thread_(new QThread(this)) {

  collection_watcher_->moveToThread(watcher_thread_);
  watcher_thread_->start(QThread::IdlePriority);

  qLog(Debug) << collection_watcher_ << "for device" << unique_id << "moved to thread" << watcher_thread_;

  collection_watcher_->set_device_name(device_manager->DeviceNameByID(unique_id));

  QObject::connect(&*collection_backend_, &CollectionBackend::DirectoryAdded, collection_watcher_, &CollectionWatcher::AddDirectory);
  QObject::connect(&*collection_backend_, &CollectionBackend::DirectoryDeleted, collection_watcher_, &CollectionWatcher::RemoveDirectory);
  QObject::connect(collection_watcher_, &CollectionWatcher::NewOrUpdatedSongs, &*collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(collection_watcher_, &CollectionWatcher::SongsMTimeUpdated, &*collection_backend_, &CollectionBackend::UpdateMTimesOnly);
  QObject::connect(collection_watcher_, &CollectionWatcher::SongsDeleted, &*collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(collection_watcher_, &CollectionWatcher::SongsUnavailable, &*collection_backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(collection_watcher_, &CollectionWatcher::SongsReadded, &*collection_backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(collection_watcher_, &CollectionWatcher::SubdirsDiscovered, &*collection_backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(collection_watcher_, &CollectionWatcher::SubdirsMTimeUpdated, &*collection_backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(collection_watcher_, &CollectionWatcher::CompilationsNeedUpdating, &*collection_backend_, &CollectionBackend::CompilationsNeedUpdating);
  QObject::connect(collection_watcher_, &CollectionWatcher::UpdateLastSeen, &*collection_backend_, &CollectionBackend::UpdateLastSeen);
  QObject::connect(collection_watcher_, &CollectionWatcher::ScanStarted, this, &FilesystemDevice::TaskStarted);

}

FilesystemDevice::~FilesystemDevice() {

  collection_watcher_->Abort();
  collection_watcher_->deleteLater();
  watcher_thread_->exit();
  watcher_thread_->wait();

}

bool FilesystemDevice::Init() {

  InitBackendDirectory(url_.toLocalFile(), first_time_);
  collection_model_->Init();
  return true;

}

void FilesystemDevice::CloseAsync() {
  QMetaObject::invokeMethod(this, &FilesystemDevice::Close, Qt::QueuedConnection);
}

void FilesystemDevice::Close() {

  Q_ASSERT(QThread::currentThread() == thread());

  wait_for_exit_ << &*collection_backend_ << collection_watcher_;

  QObject::disconnect(&*collection_backend_, nullptr, collection_watcher_, nullptr);
  QObject::disconnect(collection_watcher_, nullptr, &*collection_backend_, nullptr);

  QObject::connect(&*collection_backend_, &CollectionBackend::ExitFinished, this, &FilesystemDevice::ExitFinished);
  QObject::connect(collection_watcher_, &CollectionWatcher::ExitFinished, this, &FilesystemDevice::ExitFinished);
  collection_backend_->ExitAsync();
  collection_watcher_->ExitAsync();

}

void FilesystemDevice::ExitFinished() {

  QObject *obj = sender();
  if (!obj) return;
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) {
    Q_EMIT DeviceCloseFinished(unique_id());
  }

}
