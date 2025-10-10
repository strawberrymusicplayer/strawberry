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

#include "core/taskmanager.h"
#include "core/database.h"
#include "core/thread.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "tagreader/tagreaderclient.h"
#include "utilities/threadutils.h"
#include "collectionlibrary.h"
#include "collectionwatcher.h"
#include "collectionbackend.h"
#include "collectionmodel.h"
#include "constants/collectionsettings.h"

using std::make_shared;

const char *CollectionLibrary::kSongsTable = "songs";
const char *CollectionLibrary::kDirsTable = "directories";
const char *CollectionLibrary::kSubdirsTable = "subdirectories";

CollectionLibrary::CollectionLibrary(const SharedPtr<Database> database,
                                     const SharedPtr<TaskManager> task_manager,
                                     const SharedPtr<TagReaderClient> tagreader_client,
                                     const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                     QObject *parent)
    : QObject(parent),
      task_manager_(task_manager),
      tagreader_client_(tagreader_client),
      backend_(nullptr),
      model_(nullptr),
      watcher_(nullptr),
      watcher_thread_(nullptr),
      original_thread_(nullptr),
      save_playcounts_to_files_(false),
      save_ratings_to_files_(false) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  original_thread_ = thread();

  backend_ = make_shared<CollectionBackend>();
  backend()->moveToThread(database->thread());
  qLog(Debug) << &*backend_ << "moved to thread" << database->thread();

  backend_->Init(database, task_manager, Song::Source::Collection, QLatin1String(kSongsTable), QLatin1String(kDirsTable), QLatin1String(kSubdirsTable));

  model_ = new CollectionModel(backend_, albumcover_loader, this);

  full_rescan_revisions_[21] = tr("Support for sort tags artist, album, album artist, title, composer, and performer");

  ReloadSettings();

}

CollectionLibrary::~CollectionLibrary() {

  if (watcher_) {
    watcher_->Abort();
    watcher_->deleteLater();
  }
  if (watcher_thread_) {
    watcher_thread_->exit();
    watcher_thread_->wait(5000);
  }

}

void CollectionLibrary::Init() {

  watcher_ = new CollectionWatcher(Song::Source::Collection, task_manager_, tagreader_client_, backend_);
  watcher_thread_ = new Thread(this);
  watcher_thread_->setObjectName(watcher_->objectName());

  watcher_thread_->SetIoPriority(Utilities::IoPriority::IOPRIO_CLASS_IDLE);

  watcher_->moveToThread(watcher_thread_);

  qLog(Debug) << watcher_ << "moved to thread" << watcher_thread_;

  watcher_thread_->start(QThread::IdlePriority);

  QObject::connect(&*backend_, &CollectionBackend::Error, this, &CollectionLibrary::Error);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryAdded, watcher_, &CollectionWatcher::AddDirectory);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, watcher_, &CollectionWatcher::RemoveDirectory);
  QObject::connect(&*backend_, &CollectionBackend::SongsRatingChanged, this, &CollectionLibrary::SongsRatingChanged);
  QObject::connect(&*backend_, &CollectionBackend::SongsStatisticsChanged, this, &CollectionLibrary::SongsPlaycountChanged);

  QObject::connect(watcher_, &CollectionWatcher::NewOrUpdatedSongs, &*backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsMTimeUpdated, &*backend_, &CollectionBackend::UpdateMTimesOnly);
  QObject::connect(watcher_, &CollectionWatcher::SongsDeleted, &*backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsUnavailable, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SongsReadded, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsDiscovered, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsMTimeUpdated, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::CompilationsNeedUpdating, &*backend_, &CollectionBackend::CompilationsNeedUpdating);
  QObject::connect(watcher_, &CollectionWatcher::UpdateLastSeen, &*backend_, &CollectionBackend::UpdateLastSeen);

  // This will start the watcher checking for updates
  backend_->LoadDirectoriesAsync();

}

void CollectionLibrary::Exit() {

  wait_for_exit_ << &*backend_ << watcher_;

  QObject::disconnect(&*backend_, nullptr, watcher_, nullptr);
  QObject::disconnect(watcher_, nullptr, &*backend_, nullptr);

  QObject::connect(&*backend_, &CollectionBackend::ExitFinished, this, &CollectionLibrary::ExitReceived);
  QObject::connect(watcher_, &CollectionWatcher::ExitFinished, this, &CollectionLibrary::ExitReceived);
  backend_->ExitAsync();
  watcher_->Abort();
  watcher_->ExitAsync();

}

void CollectionLibrary::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void CollectionLibrary::IncrementalScan() { watcher_->IncrementalScanAsync(); }

void CollectionLibrary::FullScan() { watcher_->FullScanAsync(); }

void CollectionLibrary::StopScan() { watcher_->Stop(); }

void CollectionLibrary::Rescan(const SongList &songs) {

  qLog(Debug) << "Rescan" << songs.size() << "songs";
  if (!songs.isEmpty()) {
    watcher_->RescanSongsAsync(songs);
  }

}

void CollectionLibrary::PauseWatcher() { watcher_->SetRescanPausedAsync(true); }

void CollectionLibrary::ResumeWatcher() { watcher_->SetRescanPausedAsync(false); }

void CollectionLibrary::ReloadSettings() {

  watcher_->ReloadSettingsAsync();
  model_->ReloadSettings();

  Settings s;
  s.beginGroup(CollectionSettings::kSettingsGroup);
  save_playcounts_to_files_ = s.value(CollectionSettings::kSavePlayCounts, false).toBool();
  save_ratings_to_files_ = s.value(CollectionSettings::kSaveRatings, false).toBool();
  s.endGroup();

}

void CollectionLibrary::SyncPlaycountAndRatingToFilesAsync() {

  (void)QtConcurrent::run(&CollectionLibrary::SyncPlaycountAndRatingToFiles, this);

}

void CollectionLibrary::SyncPlaycountAndRatingToFiles() {

  const int task_id = task_manager_->StartTask(tr("Saving playcounts and ratings"));
  task_manager_->SetTaskBlocksCollectionScans(task_id);

  const SongList songs = backend_->GetAllSongs();
  const quint64 nb_songs = static_cast<quint64>(songs.size());
  quint64 i = 0;
  for (const Song &song : songs) {
    (void)tagreader_client_->SaveSongPlaycountBlocking(song.url().toLocalFile(), song.playcount());
    (void)tagreader_client_->SaveSongRatingBlocking(song.url().toLocalFile(), song.rating());
    task_manager_->SetTaskProgress(task_id, ++i, nb_songs);
  }
  task_manager_->SetTaskFinished(task_id);

}

void CollectionLibrary::SongsPlaycountChanged(const SongList &songs, const bool save_tags) const {

  if (save_tags || save_playcounts_to_files_) {
    tagreader_client_->SaveSongsPlaycountAsync(songs);
  }

}

void CollectionLibrary::SongsRatingChanged(const SongList &songs, const bool save_tags) const {

  if (save_tags || save_ratings_to_files_) {
    tagreader_client_->SaveSongsRatingAsync(songs);
  }

}
