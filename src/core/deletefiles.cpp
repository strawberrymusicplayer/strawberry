/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QThread>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QMetaObject>

#include "includes/shared_ptr.h"
#include "taskmanager.h"
#include "song.h"
#include "deletefiles.h"
#include "musicstorage.h"

namespace {
constexpr int kBatchSize = 50;
}

DeleteFiles::DeleteFiles(SharedPtr<TaskManager> task_manager, SharedPtr<MusicStorage> storage, const bool use_trash, QObject *parent)
    : QObject(parent),
      thread_(nullptr),
      task_manager_(task_manager),
      storage_(storage),
      use_trash_(use_trash),
      started_(false),
      task_id_(0),
      progress_(0) {
  original_thread_ = thread();
}

DeleteFiles::~DeleteFiles() = default;

void DeleteFiles::Start(const SongList &songs) {

  if (thread_) return;

  songs_ = songs;

  task_id_ = task_manager_->StartTask(tr("Deleting files"));
  task_manager_->SetTaskBlocksCollectionScans(task_id_);

  thread_ = new QThread(this);
  QObject::connect(thread_, &QThread::started, this, &DeleteFiles::ProcessSomeFiles);

  moveToThread(thread_);
  thread_->start();

}

void DeleteFiles::Start(const QStringList &filenames) {

  SongList songs;
  songs.reserve(filenames.count());
  for (const QString &filename : filenames) {
    Song song;
    song.set_url(QUrl::fromLocalFile(filename));
    songs << song;
  }

  Start(songs);

}

void DeleteFiles::ProcessSomeFiles() {

  if (!started_) {
    storage_->StartDelete();
    started_ = true;
  }

  // None left?
  if (progress_ >= songs_.count()) {
    task_manager_->SetTaskProgress(task_id_, static_cast<quint64>(progress_), static_cast<quint64>(songs_.count()));

    QString error_text;
    storage_->FinishCopy(songs_with_errors_.isEmpty(), error_text);

    task_manager_->SetTaskFinished(task_id_);

    Q_EMIT Finished(songs_with_errors_);

    // Move back to the original thread so deleteLater() can get called in the main thread's event loop
    moveToThread(original_thread_);
    deleteLater();

    // Stop this thread
    thread_->quit();
    return;
  }

  // We process files in batches so we can be cancelled part-way through.

  const qint64 n = qMin(static_cast<qint64>(songs_.count()), static_cast<qint64>(progress_ + kBatchSize));
  for (; progress_ < n; ++progress_) {
    task_manager_->SetTaskProgress(task_id_, static_cast<quint64>(progress_), static_cast<quint64>(songs_.count()));

    const Song song = songs_.value(progress_);

    MusicStorage::DeleteJob job;
    job.metadata_ = song;
    job.use_trash_ = use_trash_;

    if (!storage_->DeleteFromStorage(job)) {
      songs_with_errors_ << song;
    }
  }

  QMetaObject::invokeMethod(this, &DeleteFiles::ProcessSomeFiles);

}
