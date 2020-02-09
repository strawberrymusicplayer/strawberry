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

#include "config.h"

#include <assert.h>

#include <QThread>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"

#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionwatcher.h"
#include "connecteddevice.h"
#include "devicemanager.h"
#include "filesystemdevice.h"

class DeviceLister;

FilesystemDevice::FilesystemDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, DeviceManager *manager, Application *app, int database_id, bool first_time)
      : FilesystemMusicStorage(url.toLocalFile()),
      ConnectedDevice(url, lister, unique_id, manager, app, database_id, first_time),
      watcher_(new CollectionWatcher(Song::Source_Device)), watcher_thread_(new QThread(this))
  {

  watcher_->moveToThread(watcher_thread_);
  watcher_thread_->start(QThread::IdlePriority);
  qLog(Debug) << watcher_ << "for device" << unique_id << "moved to thread" << watcher_thread_;

  watcher_->set_device_name(manager->DeviceNameByID(unique_id));
  watcher_->set_backend(backend_);
  watcher_->set_task_manager(app_->task_manager());

  connect(backend_, SIGNAL(DirectoryDiscovered(Directory, SubdirectoryList)), watcher_, SLOT(AddDirectory(Directory, SubdirectoryList)));
  connect(backend_, SIGNAL(DirectoryDeleted(Directory)), watcher_, SLOT(RemoveDirectory(Directory)));
  connect(watcher_, SIGNAL(NewOrUpdatedSongs(SongList)), backend_, SLOT(AddOrUpdateSongs(SongList)));
  connect(watcher_, SIGNAL(SongsMTimeUpdated(SongList)), backend_, SLOT(UpdateMTimesOnly(SongList)));
  connect(watcher_, SIGNAL(SongsDeleted(SongList)), backend_, SLOT(DeleteSongs(SongList)));
  connect(watcher_, SIGNAL(SubdirsDiscovered(SubdirectoryList)), backend_, SLOT(AddOrUpdateSubdirs(SubdirectoryList)));
  connect(watcher_, SIGNAL(SubdirsMTimeUpdated(SubdirectoryList)), backend_, SLOT(AddOrUpdateSubdirs(SubdirectoryList)));
  connect(watcher_, SIGNAL(CompilationsNeedUpdating()), backend_, SLOT(UpdateCompilations()));
  connect(watcher_, SIGNAL(ScanStarted(int)), SIGNAL(TaskStarted(int)));

}

FilesystemDevice::~FilesystemDevice() {

  watcher_->Stop();
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
  metaObject()->invokeMethod(this, "Close", Qt::QueuedConnection);
}

void FilesystemDevice::Close() {

  assert(QThread::currentThread() == thread());

  wait_for_exit_ << backend_ << watcher_;

  disconnect(backend_, 0, watcher_, 0);
  disconnect(watcher_, 0, backend_, 0);

  connect(backend_, SIGNAL(ExitFinished()), this, SLOT(ExitFinished()));
  connect(watcher_, SIGNAL(ExitFinished()), this, SLOT(ExitFinished()));
  backend_->ExitAsync();
  watcher_->ExitAsync();

}

void FilesystemDevice::ExitFinished() {

  QObject *obj = static_cast<QObject*>(sender());
  if (!obj) return;
  disconnect(obj, 0, this, 0);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) {
    emit CloseFinished(unique_id());
  }

}
