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

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/song.h"

#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionwatcher.h"
#include "connecteddevice.h"
#include "devicemanager.h"
#include "filesystemdevice.h"

class DeviceLister;

FilesystemDevice::FilesystemDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, SharedPtr<DeviceManager> manager, Application *app, const int database_id, const bool first_time, QObject *parent)
    : FilesystemMusicStorage(Song::Source::Device, url.toLocalFile()),
      ConnectedDevice(url, lister, unique_id, manager, app, database_id, first_time, parent),
      watcher_(new CollectionWatcher(Song::Source::Device)),
      watcher_thread_(new QThread(this)) {

  watcher_->moveToThread(watcher_thread_);
  watcher_thread_->start(QThread::IdlePriority);
  qLog(Debug) << watcher_ << "for device" << unique_id << "moved to thread" << watcher_thread_;

  watcher_->set_device_name(manager->DeviceNameByID(unique_id));
  watcher_->set_backend(backend_);
  watcher_->set_task_manager(app_->task_manager());

  QObject::connect(&*backend_, &CollectionBackend::DirectoryAdded, watcher_, &CollectionWatcher::AddDirectory);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, watcher_, &CollectionWatcher::RemoveDirectory);
  QObject::connect(watcher_, &CollectionWatcher::NewOrUpdatedSongs, &*backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsMTimeUpdated, &*backend_, &CollectionBackend::UpdateMTimesOnly);
  QObject::connect(watcher_, &CollectionWatcher::SongsDeleted, &*backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsUnavailable, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SongsReadded, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsDiscovered, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsMTimeUpdated, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::CompilationsNeedUpdating, &*backend_, &CollectionBackend::CompilationsNeedUpdating);
  QObject::connect(watcher_, &CollectionWatcher::UpdateLastSeen, &*backend_, &CollectionBackend::UpdateLastSeen);
  QObject::connect(watcher_, &CollectionWatcher::ScanStarted, this, &FilesystemDevice::TaskStarted);

}

FilesystemDevice::~FilesystemDevice() {

  watcher_->Abort();
  watcher_->deleteLater();
  watcher_thread_->exit();
  watcher_thread_->wait();

}

bool FilesystemDevice::Init() {

  InitBackendDirectory(url_.toLocalFile(), first_time_);
  model_->Init();
  return true;

}

void FilesystemDevice::CloseAsync() {
  QMetaObject::invokeMethod(this, &FilesystemDevice::Close, Qt::QueuedConnection);
}

void FilesystemDevice::Close() {

  Q_ASSERT(QThread::currentThread() == thread());

  wait_for_exit_ << &*backend_ << watcher_;

  QObject::disconnect(&*backend_, nullptr, watcher_, nullptr);
  QObject::disconnect(watcher_, nullptr, &*backend_, nullptr);

  QObject::connect(&*backend_, &CollectionBackend::ExitFinished, this, &FilesystemDevice::ExitFinished);
  QObject::connect(watcher_, &CollectionWatcher::ExitFinished, this, &FilesystemDevice::ExitFinished);
  backend_->ExitAsync();
  watcher_->ExitAsync();

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
