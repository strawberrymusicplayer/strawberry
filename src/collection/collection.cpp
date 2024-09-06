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

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QList>
#include <QSettings>
#include <QtConcurrentRun>

#include "core/application.h"
#include "core/taskmanager.h"
#include "core/database.h"
#include "core/tagreaderclient.h"
#include "core/thread.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/threadutils.h"
#include "collection.h"
#include "collectionwatcher.h"
#include "collectionbackend.h"
#include "collectionmodel.h"
#include "scrobbler/lastfmimport.h"
#include "settings/collectionsettingspage.h"

using std::make_shared;

const char *SCollection::kSongsTable = "songs";
const char *SCollection::kDirsTable = "directories";
const char *SCollection::kSubdirsTable = "subdirectories";

SCollection::SCollection(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      backend_(nullptr),
      model_(nullptr),
      watcher_(nullptr),
      watcher_thread_(nullptr),
      original_thread_(nullptr),
      save_playcounts_to_files_(false),
      save_ratings_to_files_(false) {

  setObjectName(QLatin1String(metaObject()->className()));

  original_thread_ = thread();

  backend_ = make_shared<CollectionBackend>();
  backend()->moveToThread(app->database()->thread());
  qLog(Debug) << &*backend_ << "moved to thread" << app->database()->thread();

  backend_->Init(app->database(), app->task_manager(), Song::Source::Collection, QLatin1String(kSongsTable), QLatin1String(kDirsTable), QLatin1String(kSubdirsTable));

  model_ = new CollectionModel(backend_, app_, this);

  ReloadSettings();

}

SCollection::~SCollection() {

  if (watcher_) {
    watcher_->Abort();
    watcher_->deleteLater();
  }
  if (watcher_thread_) {
    watcher_thread_->exit();
    watcher_thread_->wait(5000);
  }

}

void SCollection::Init() {

  watcher_ = new CollectionWatcher(Song::Source::Collection);
  watcher_thread_ = new Thread(this);
  watcher_thread_->setObjectName(watcher_->objectName());

  watcher_thread_->SetIoPriority(Utilities::IoPriority::IOPRIO_CLASS_IDLE);

  watcher_->moveToThread(watcher_thread_);

  qLog(Debug) << watcher_ << "moved to thread" << watcher_thread_;

  watcher_thread_->start(QThread::IdlePriority);

  watcher_->set_backend(backend_);
  watcher_->set_task_manager(app_->task_manager());

  QObject::connect(&*backend_, &CollectionBackend::Error, this, &SCollection::Error);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryAdded, watcher_, &CollectionWatcher::AddDirectory);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, watcher_, &CollectionWatcher::RemoveDirectory);
  QObject::connect(&*backend_, &CollectionBackend::SongsRatingChanged, this, &SCollection::SongsRatingChanged);
  QObject::connect(&*backend_, &CollectionBackend::SongsStatisticsChanged, this, &SCollection::SongsPlaycountChanged);

  QObject::connect(watcher_, &CollectionWatcher::NewOrUpdatedSongs, &*backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsMTimeUpdated, &*backend_, &CollectionBackend::UpdateMTimesOnly);
  QObject::connect(watcher_, &CollectionWatcher::SongsDeleted, &*backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsUnavailable, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SongsReadded, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsDiscovered, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsMTimeUpdated, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::CompilationsNeedUpdating, &*backend_, &CollectionBackend::CompilationsNeedUpdating);
  QObject::connect(watcher_, &CollectionWatcher::UpdateLastSeen, &*backend_, &CollectionBackend::UpdateLastSeen);

  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdateLastPlayed, &*backend_, &CollectionBackend::UpdateLastPlayed);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdatePlayCount, &*backend_, &CollectionBackend::UpdatePlayCount);

  // This will start the watcher checking for updates
  backend_->LoadDirectoriesAsync();

}

void SCollection::Exit() {

  wait_for_exit_ << &*backend_ << watcher_;

  QObject::disconnect(&*backend_, nullptr, watcher_, nullptr);
  QObject::disconnect(watcher_, nullptr, &*backend_, nullptr);

  QObject::connect(&*backend_, &CollectionBackend::ExitFinished, this, &SCollection::ExitReceived);
  QObject::connect(watcher_, &CollectionWatcher::ExitFinished, this, &SCollection::ExitReceived);
  backend_->ExitAsync();
  watcher_->Abort();
  watcher_->ExitAsync();

}

void SCollection::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void SCollection::IncrementalScan() { watcher_->IncrementalScanAsync(); }

void SCollection::FullScan() { watcher_->FullScanAsync(); }

void SCollection::StopScan() { watcher_->Stop(); }

void SCollection::Rescan(const SongList &songs) {

  qLog(Debug) << "Rescan" << songs.size() << "songs";
  if (!songs.isEmpty()) {
    watcher_->RescanSongsAsync(songs);
  }

}

void SCollection::PauseWatcher() { watcher_->SetRescanPausedAsync(true); }

void SCollection::ResumeWatcher() { watcher_->SetRescanPausedAsync(false); }

void SCollection::ReloadSettings() {

  watcher_->ReloadSettingsAsync();
  model_->ReloadSettings();

  Settings s;
  s.beginGroup(CollectionSettingsPage::kSettingsGroup);
  save_playcounts_to_files_ = s.value("save_playcounts", false).toBool();
  save_ratings_to_files_ = s.value("save_ratings", false).toBool();
  s.endGroup();

}

void SCollection::SyncPlaycountAndRatingToFilesAsync() {

  (void)QtConcurrent::run(&SCollection::SyncPlaycountAndRatingToFiles, this);

}

void SCollection::SyncPlaycountAndRatingToFiles() {

  const int task_id = app_->task_manager()->StartTask(tr("Saving playcounts and ratings"));
  app_->task_manager()->SetTaskBlocksCollectionScans(task_id);

  const SongList songs = backend_->GetAllSongs();
  const qint64 nb_songs = songs.size();
  int i = 0;
  for (const Song &song : songs) {
    (void)TagReaderClient::Instance()->SaveSongPlaycountBlocking(song.url().toLocalFile(), song.playcount());
    (void)TagReaderClient::Instance()->SaveSongRatingBlocking(song.url().toLocalFile(), song.rating());
    app_->task_manager()->SetTaskProgress(task_id, ++i, nb_songs);
  }
  app_->task_manager()->SetTaskFinished(task_id);

}

void SCollection::SongsPlaycountChanged(const SongList &songs, const bool save_tags) {

  if (save_tags || save_playcounts_to_files_) {
    app_->tag_reader_client()->SaveSongsPlaycount(songs);
  }

}

void SCollection::SongsRatingChanged(const SongList &songs, const bool save_tags) {

  if (save_tags || save_ratings_to_files_) {
    app_->tag_reader_client()->SaveSongsRating(songs);
  }

}
