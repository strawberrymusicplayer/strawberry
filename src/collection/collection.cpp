/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QObject>
#include <QThread>
#include <QList>
#include <QtDebug>

#include "core/application.h"
#include "core/database.h"
#include "core/player.h"
#include "core/tagreaderclient.h"
#include "core/thread.h"
#include "core/utilities.h"
#include "core/song.h"
#include "core/logging.h"
#include "collection.h"
#include "collectionwatcher.h"
#include "collectionbackend.h"
#include "collectionmodel.h"
#include "playlist/playlistmanager.h"
#include "scrobbler/lastfmimport.h"

const char *SCollection::kSongsTable = "songs";
const char *SCollection::kDirsTable = "directories";
const char *SCollection::kSubdirsTable = "subdirectories";
const char *SCollection::kFtsTable = "songs_fts";

SCollection::SCollection(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      backend_(nullptr),
      model_(nullptr),
      watcher_(nullptr),
      watcher_thread_(nullptr),
      original_thread_(nullptr) {

  original_thread_ = thread();

  backend_ = new CollectionBackend();
  backend()->moveToThread(app->database()->thread());
  qLog(Debug) << backend_ << "moved to thread" << app->database()->thread();

  backend_->Init(app->database(), Song::Source_Collection, kSongsTable, kDirsTable, kSubdirsTable, kFtsTable);

  model_ = new CollectionModel(backend_, app_, this);

  ReloadSettings();

}

SCollection::~SCollection() {

  if (watcher_) {
    watcher_->Stop();
    watcher_->deleteLater();
  }
  if (watcher_thread_) {
    watcher_thread_->exit();
    watcher_thread_->wait(5000 /* five seconds */);
  }
  backend_->deleteLater();

}

void SCollection::Init() {

  watcher_ = new CollectionWatcher(Song::Source_Collection);
  watcher_thread_ = new Thread(this);
  watcher_thread_->SetIoPriority(Utilities::IOPRIO_CLASS_IDLE);

  watcher_->moveToThread(watcher_thread_);
  watcher_thread_->start(QThread::IdlePriority);
  qLog(Debug) << watcher_ << "moved to thread" << watcher_thread_;

  watcher_->set_backend(backend_);
  watcher_->set_task_manager(app_->task_manager());

  connect(backend_, SIGNAL(DirectoryDiscovered(Directory, SubdirectoryList)), watcher_, SLOT(AddDirectory(Directory, SubdirectoryList)));
  connect(backend_, SIGNAL(DirectoryDeleted(Directory)), watcher_, SLOT(RemoveDirectory(Directory)));
  connect(watcher_, SIGNAL(NewOrUpdatedSongs(SongList)), backend_, SLOT(AddOrUpdateSongs(SongList)));
  connect(watcher_, SIGNAL(SongsMTimeUpdated(SongList)), backend_, SLOT(UpdateMTimesOnly(SongList)));
  connect(watcher_, SIGNAL(SongsDeleted(SongList)), backend_, SLOT(DeleteSongs(SongList)));
  connect(watcher_, SIGNAL(SongsUnavailable(SongList)), backend_, SLOT(MarkSongsUnavailable(SongList)));
  connect(watcher_, SIGNAL(SongsReadded(SongList, bool)), backend_, SLOT(MarkSongsUnavailable(SongList, bool)));
  connect(watcher_, SIGNAL(SubdirsDiscovered(SubdirectoryList)), backend_, SLOT(AddOrUpdateSubdirs(SubdirectoryList)));
  connect(watcher_, SIGNAL(SubdirsMTimeUpdated(SubdirectoryList)), backend_, SLOT(AddOrUpdateSubdirs(SubdirectoryList)));
  connect(watcher_, SIGNAL(CompilationsNeedUpdating()), backend_, SLOT(UpdateCompilations()));
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), SLOT(CurrentSongChanged(Song)));
  connect(app_->player(), SIGNAL(Stopped()), SLOT(Stopped()));

  connect(app_->lastfm_import(), SIGNAL(UpdateLastPlayed(QString, QString, QString, qint64)), backend_, SLOT(UpdateLastPlayed(QString, QString, QString, qint64)));
  connect(app_->lastfm_import(), SIGNAL(UpdatePlayCount(QString, QString, int)), backend_, SLOT(UpdatePlayCount(QString, QString, int)));

  // This will start the watcher checking for updates
  backend_->LoadDirectoriesAsync();

}

void SCollection::Exit() {

  wait_for_exit_ << backend_ << watcher_;

  disconnect(backend_, nullptr, watcher_, nullptr);
  disconnect(watcher_, nullptr, backend_, nullptr);

  connect(backend_, SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  connect(watcher_, SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  backend_->ExitAsync();
  watcher_->ExitAsync();

}

void SCollection::ExitReceived() {

  QObject *obj = qobject_cast<QObject*>(sender());
  disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) emit ExitFinished();

}

void SCollection::IncrementalScan() { watcher_->IncrementalScanAsync(); }

void SCollection::FullScan() { watcher_->FullScanAsync(); }

void SCollection::AbortScan() { watcher_->Stop(); }

void SCollection::Rescan(const SongList &songs) {

    qLog(Debug) << "Rescan" << songs.size() << "songs";
    if (!songs.isEmpty()) watcher_->RescanTracksAsync(songs);

}

void SCollection::PauseWatcher() { watcher_->SetRescanPausedAsync(true); }

void SCollection::ResumeWatcher() { watcher_->SetRescanPausedAsync(false); }

void SCollection::ReloadSettings() {

  watcher_->ReloadSettingsAsync();
  model_->ReloadSettings();

}

void SCollection::Stopped() {

  CurrentSongChanged(Song());
}

void SCollection::CurrentSongChanged(const Song &song) {  // FIXME

  Q_UNUSED(song);

  TagReaderReply *reply = nullptr;

  if (reply) {
    connect(reply, SIGNAL(Finished(bool)), reply, SLOT(deleteLater()));
  }

}
