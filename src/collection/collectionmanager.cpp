/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QFuture>
#include <QFutureWatcher>

#include "core/taskmanager.h"
#include "core/database.h"
#include "core/thread.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "tagreader/tagreaderclient.h"
#include "utilities/threadutils.h"
#include "collectionmanager.h"
#include "collectionwatcher.h"
#include "collectionbackend.h"
#include "collectionmodel.h"
#include "constants/collectionsettings.h"

using std::make_shared;

const char *CollectionManager::kSongsTable = "songs";
const char *CollectionManager::kDirsTable = "directories";
const char *CollectionManager::kSubdirsTable = "subdirectories";

CollectionManager::CollectionManager(const SharedPtr<Database> database,
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
      original_thread_(thread()),
      save_playcounts_to_files_(false),
      save_ratings_to_files_(false),
      save_lyrics_to_files_(false) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  backend_ = make_shared<CollectionBackend>();
  backend()->moveToThread(database->thread());
  qLog(Debug) << &*backend_ << "moved to thread" << database->thread();

  backend_->Init(database, task_manager, Song::Source::Collection, QLatin1String(kSongsTable), QLatin1String(kDirsTable), QLatin1String(kSubdirsTable));

  model_ = new CollectionModel(backend_, albumcover_loader, this);

  full_rescan_revisions_[21] = tr("Support for sort tags artist, album, album artist, title, composer and performer");
  full_rescan_revisions_[26] = tr("Support for synchronized lyrics");
  full_rescan_revisions_[27] = tr("Support for synchronized lyrics from LRC files");

  ReloadSettings();

}

CollectionManager::~CollectionManager() {

  if (watcher_) {
    watcher_->Abort();
    watcher_->deleteLater();
  }
  if (watcher_thread_) {
    watcher_thread_->exit();
    watcher_thread_->wait(5000);
  }

}

void CollectionManager::Init() {

  watcher_ = new CollectionWatcher(Song::Source::Collection, task_manager_, tagreader_client_, backend_);
  watcher_thread_ = new Thread(this);
  watcher_thread_->setObjectName(watcher_->objectName());

  watcher_thread_->SetIoPriority(Utilities::IoPriority::IOPRIO_CLASS_IDLE);

  watcher_->moveToThread(watcher_thread_);

  qLog(Debug) << watcher_ << "moved to thread" << watcher_thread_;

  watcher_thread_->start(QThread::IdlePriority);

  QObject::connect(&*backend_, &CollectionBackend::Error, this, &CollectionManager::Error);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryAdded, watcher_, &CollectionWatcher::AddDirectory);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, watcher_, &CollectionWatcher::RemoveDirectory);
  QObject::connect(&*backend_, &CollectionBackend::SongsRatingChanged, this, &CollectionManager::SongsRatingChanged);
  QObject::connect(&*backend_, &CollectionBackend::SongsStatisticsChanged, this, &CollectionManager::SongsPlaycountChanged);
  QObject::connect(&*backend_, &CollectionBackend::SongsLyricsChanged, this, &CollectionManager::SongsLyricsChanged);

  QObject::connect(watcher_, &CollectionWatcher::NewOrUpdatedSongs, &*backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsMTimeUpdated, &*backend_, &CollectionBackend::UpdateMTimesOnly);
  QObject::connect(watcher_, &CollectionWatcher::SongsDeleted, &*backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(watcher_, &CollectionWatcher::SongsUnavailable, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SongsReadded, &*backend_, &CollectionBackend::MarkSongsUnavailable);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsDiscovered, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsMTimeUpdated, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::SubdirsDeleted, &*backend_, &CollectionBackend::DeleteSubdirs);
  QObject::connect(watcher_, &CollectionWatcher::CompilationsNeedUpdating, &*backend_, &CollectionBackend::CompilationsNeedUpdating);
  QObject::connect(watcher_, &CollectionWatcher::UpdateLastSeen, &*backend_, &CollectionBackend::UpdateLastSeen);

  // This will start the watcher checking for updates
  backend_->LoadDirectoriesAsync();

}

void CollectionManager::Exit() {

  wait_for_exit_ << &*backend_ << watcher_;

  QObject::disconnect(&*backend_, nullptr, watcher_, nullptr);
  QObject::disconnect(watcher_, nullptr, &*backend_, nullptr);

  QObject::connect(&*backend_, &CollectionBackend::ExitFinished, this, &CollectionManager::ExitReceived);
  QObject::connect(watcher_, &CollectionWatcher::ExitFinished, this, &CollectionManager::ExitReceived);
  backend_->ExitAsync();
  watcher_->Abort();
  watcher_->ExitAsync();

}

void CollectionManager::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void CollectionManager::IncrementalScan() { watcher_->IncrementalScanAsync(); }

void CollectionManager::FullScan() { watcher_->FullScanAsync(); }

void CollectionManager::StopScan() { watcher_->Stop(); }

void CollectionManager::Rescan(const SongList &songs) {

  qLog(Debug) << "Rescan" << songs.size() << "songs";
  if (!songs.isEmpty()) {
    watcher_->RescanSongsAsync(songs);
  }

}

void CollectionManager::PauseWatcher() { watcher_->SetRescanPausedAsync(true); }

void CollectionManager::ResumeWatcher() { watcher_->SetRescanPausedAsync(false); }

void CollectionManager::ReloadSettings() {

  watcher_->ReloadSettingsAsync();
  model_->ReloadSettings();

  Settings s;
  s.beginGroup(CollectionSettings::kSettingsGroup);
  save_playcounts_to_files_ = s.value(CollectionSettings::kSavePlayCounts, CollectionSettings::kDefaultSavePlayCounts).toBool();
  save_ratings_to_files_ = s.value(CollectionSettings::kSaveRatings, CollectionSettings::kDefaultSaveRatings).toBool();
  save_lyrics_to_files_ = s.value(CollectionSettings::kSaveLyrics, CollectionSettings::kDefaultSaveLyrics).toBool();
  s.endGroup();

}

void CollectionManager::CurrentSongChanged(const Song &song) {

  current_song_url_ = song.url();

  if (!pending_song_saves_.isEmpty()) {
    SavePendingSongSaves();
  }

}

void CollectionManager::Stopped() {

  current_song_url_ = QUrl();

  if (!pending_song_saves_.isEmpty()) {
    SavePendingSongSaves();
  }

}

void CollectionManager::SyncPlaycountAndRatingToFilesAsync() {

  (void)QtConcurrent::run(&CollectionManager::SyncPlaycountAndRatingToFiles, this);

}

void CollectionManager::SyncPlaycountAndRatingToFiles() {

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

void CollectionManager::SongsPlaycountChanged(const SongList &songs, const bool save_tags) {

  if (save_tags || save_playcounts_to_files_) {
    SongList songs_to_save_now;
    for (const Song &song : songs) {
      if (IsCurrentSongSaveDeferred(song, current_song_url_)) {
        qLog(Debug) << "Deferring playcount save for currently playing file" << song.url().toLocalFile();
        if (pending_song_saves_.contains(song.url())) {
          SharedPtr<PendingSongSave> pending_song_save = pending_song_saves_.value(song.url());
          pending_song_save->save_playcount = true;
          pending_song_save->song.set_playcount(song.playcount());
        }
        else {
          SharedPtr<PendingSongSave> pending_song_save = make_shared<PendingSongSave>();
          pending_song_save->save_playcount = true;
          pending_song_save->song = song;
          pending_song_saves_.insert(song.url(), pending_song_save);
        }
      }
      else {
        songs_to_save_now << song;
      }
    }
    if (!songs_to_save_now.isEmpty()) {
      tagreader_client_->SaveSongsPlaycountAsync(songs_to_save_now);
    }
  }

}

void CollectionManager::SongsRatingChanged(const SongList &songs, const bool save_tags) {

  if (save_tags || save_ratings_to_files_) {
    SongList songs_to_save_now;
    for (const Song &song : songs) {
      if (IsCurrentSongSaveDeferred(song, current_song_url_)) {
        qLog(Debug) << "Deferring rating save for currently playing file" << song.url().toLocalFile();
        if (pending_song_saves_.contains(song.url())) {
          SharedPtr<PendingSongSave> pending_song_save = pending_song_saves_.value(song.url());
          pending_song_save->save_rating = true;
          pending_song_save->song.set_rating(song.rating());
        }
        else {
          SharedPtr<PendingSongSave> pending_song_save = make_shared<PendingSongSave>();
          pending_song_save->save_rating = true;
          pending_song_save->song = song;
          pending_song_saves_.insert(song.url(), pending_song_save);
        }
      }
      else {
        songs_to_save_now << song;
      }
    }
    if (!songs_to_save_now.isEmpty()) {
      tagreader_client_->SaveSongsRatingAsync(songs_to_save_now);
    }
  }

}

void CollectionManager::SaveLyrics(const Song &song, const QString &lyrics, const QString &synced_lyrics) {

  if (!song.is_local_collection_song() || song.id() == -1 || (lyrics.isEmpty() && synced_lyrics.isEmpty())) return;

  // Always save to the database first, the storage type is changed to the tags once they have been written successfully.
  // Only if the song still has no lyrics, they could have been added while searching.
  if (!lyrics.isEmpty()) {
    backend_->UpdateSongLyricsAsync(song.id(), false, lyrics, Song::StorageType::Database, true, save_lyrics_to_files_);
  }
  if (!synced_lyrics.isEmpty()) {
    backend_->UpdateSongLyricsAsync(song.id(), true, synced_lyrics, Song::StorageType::Database, true, save_lyrics_to_files_);
  }

}

void CollectionManager::SongsLyricsChanged(const SongList &songs, const bool save_lyrics_tags, const bool save_synced_lyrics_tags) {

  if (!save_lyrics_tags && !save_synced_lyrics_tags) return;

  for (const Song &song : songs) {

    // CUE tracks share one media file, so their lyrics are kept in the database, the tags would be for all tracks.
    if (!song.url().isLocalFile() || song.has_cue()) continue;

    // Only lyrics just saved to the database are written, and only when the file type can store them in the tags.
    // SaveLyricsToFile() saves all lyrics with the storage type set to the tags, so only the lyrics to save have it, the other lyrics in the song could already be stored in the tags.
    Song song_to_save(song);
    const bool save_lyrics = save_lyrics_tags && song.lyrics_storage_type() == Song::StorageType::Database && !song.lyrics().isEmpty() && song.lyrics_supported();
    const bool save_synced_lyrics = save_synced_lyrics_tags && song.synced_lyrics_storage_type() == Song::StorageType::Database && !song.synced_lyrics().isEmpty() && song.synced_lyrics_supported();
    song_to_save.set_lyrics_storage_type(save_lyrics ? Song::StorageType::Tag : Song::StorageType::None);
    song_to_save.set_synced_lyrics_storage_type(save_synced_lyrics ? Song::StorageType::Tag : Song::StorageType::None);
    if (!save_lyrics && !save_synced_lyrics) continue;

    if (IsCurrentSongSaveDeferred(song, current_song_url_)) {
      qLog(Debug) << "Deferring lyrics save for currently playing file" << song.url().toLocalFile();
      if (pending_song_saves_.contains(song.url())) {
        // Only take the lyrics saved now, so lyrics already waiting to be saved are kept.
        // If no lyrics were waiting to be saved, the song is from a playcount or rating save, where lyrics could already be stored in the tags, so none of them are to be saved yet.
        SharedPtr<PendingSongSave> pending_song_save = pending_song_saves_.value(song.url());
        if (!pending_song_save->save_lyrics) {
          pending_song_save->song.set_lyrics_storage_type(Song::StorageType::None);
          pending_song_save->song.set_synced_lyrics_storage_type(Song::StorageType::None);
        }
        pending_song_save->save_lyrics = true;
        if (save_lyrics) {
          pending_song_save->song.set_lyrics(song_to_save.lyrics());
          pending_song_save->song.set_lyrics_storage_type(song_to_save.lyrics_storage_type());
        }
        if (save_synced_lyrics) {
          pending_song_save->song.set_synced_lyrics(song_to_save.synced_lyrics());
          pending_song_save->song.set_synced_lyrics_storage_type(song_to_save.synced_lyrics_storage_type());
        }
      }
      else {
        SharedPtr<PendingSongSave> pending_song_save = make_shared<PendingSongSave>();
        pending_song_save->save_lyrics = true;
        pending_song_save->song = song_to_save;
        pending_song_saves_.insert(song.url(), pending_song_save);
      }
    }
    else {
      SaveLyricsToFile(song_to_save);
    }

  }

}

Song CollectionManager::CurrentLyricsToSave(const SharedPtr<CollectionBackend> &backend, const Song &song) {

  // Called by QtConcurrentRun, so the collection is not queried in the GUI thread.
  // Static and given the backend, so it does not depend on CollectionManager still existing, playback is often stopped when exiting.
  // The lyrics could have been changed while the save was deferred, for example in the tag editor, so only save lyrics which are still the same in the collection and still only stored in the database.
  // Otherwise the tags would be overwritten with older lyrics.
  const Song collection_song = backend->GetSongById(song.id());
  Song song_to_save(song);
  if (song.lyrics_storage_type() == Song::StorageType::Tag && (!collection_song.is_valid() || collection_song.lyrics() != song.lyrics() || collection_song.lyrics_storage_type() != Song::StorageType::Database)) {
    qLog(Debug) << "Lyrics for" << song.url().toLocalFile() << "changed while the save was deferred, not saving them";
    // Anything but the tags, SaveLyricsToFile() only saves lyrics with the storage type set to the tags.
    song_to_save.set_lyrics_storage_type(Song::StorageType::Database);
  }
  if (song.synced_lyrics_storage_type() == Song::StorageType::Tag && (!collection_song.is_valid() || collection_song.synced_lyrics() != song.synced_lyrics() || collection_song.synced_lyrics_storage_type() != Song::StorageType::Database)) {
    qLog(Debug) << "Synchronized lyrics for" << song.url().toLocalFile() << "changed while the save was deferred, not saving them";
    song_to_save.set_synced_lyrics_storage_type(Song::StorageType::Database);
  }

  return song_to_save;

}

void CollectionManager::SaveLyricsToFile(const Song &song) {

  // Only the lyrics with the storage type set to the tags are saved, without writing the other tags.
  const QString filename = song.url().toLocalFile();
  const int song_id = song.id();

  if (song.lyrics_storage_type() == Song::StorageType::Tag) {
    const QString lyrics = song.lyrics();
    TagReaderReplyPtr reply = tagreader_client_->SaveSongLyricsAsync(filename, lyrics);
    SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, filename, song_id, lyrics, connection]() {
      QObject::disconnect(*connection);
      if (!reply->success()) {
        qLog(Error) << "Could not save lyrics to" << filename << reply->error();
        return;
      }
      // The lyrics are in the tags now, update where they are stored, unless they were changed in the meantime.
      backend_->UpdateSongLyricsStorageTypeAsync(song_id, false, lyrics, Song::StorageType::Database, Song::StorageType::Tag);
    }, Qt::QueuedConnection);
  }

  if (song.synced_lyrics_storage_type() == Song::StorageType::Tag) {
    const QString synced_lyrics = song.synced_lyrics();
    TagReaderReplyPtr reply = tagreader_client_->SaveSongSyncedLyricsAsync(filename, synced_lyrics);
    SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, filename, song_id, synced_lyrics, connection]() {
      QObject::disconnect(*connection);
      if (!reply->success()) {
        qLog(Error) << "Could not save synchronized lyrics to" << filename << reply->error();
        return;
      }
      // The synchronized lyrics are in the tags now, update where they are stored, unless they were changed in the meantime.
      backend_->UpdateSongLyricsStorageTypeAsync(song_id, true, synced_lyrics, Song::StorageType::Database, Song::StorageType::Tag);
    }, Qt::QueuedConnection);
  }

}

bool CollectionManager::IsCurrentSongSaveDeferred(const Song &song, const QUrl &current_song_url) {

  // Writing to these file types while they are playing can interrupt playback, so the save is deferred until the song is no longer playing.
  return song.url().isLocalFile() && song.url() == current_song_url && (song.filetype() == Song::FileType::OggFlac || song.filetype() == Song::FileType::OggVorbis || song.filetype() == Song::FileType::OggOpus || song.filetype() == Song::FileType::MPEG);

}

void CollectionManager::SavePendingSongSaves() {

  for (QMap<QUrl, SharedPtr<PendingSongSave>>::iterator it = pending_song_saves_.begin(); it != pending_song_saves_.end();) {
    const QUrl url = it.key();
    SharedPtr<PendingSongSave> pending_song_save = it.value();
    if (url == current_song_url_) {
      ++it;
      continue;
    }
    qLog(Debug) << "Saving deferred playcount, rating or lyrics for" << url.toLocalFile();
    if (pending_song_save->save_playcount) {
      tagreader_client_->SaveSongsPlaycountAsync(SongList() << pending_song_save->song);
    }
    if (pending_song_save->save_rating) {
      tagreader_client_->SaveSongsRatingAsync(SongList() << pending_song_save->song);
    }
    if (pending_song_save->save_lyrics) {
      // Check the lyrics against the collection outside the GUI thread, and save them when that is done.
      QFuture<Song> future = QtConcurrent::run([backend = backend_, song = pending_song_save->song]() { return CurrentLyricsToSave(backend, song); });
      QFutureWatcher<Song> *watcher = new QFutureWatcher<Song>(this);
      QObject::connect(watcher, &QFutureWatcher<Song>::finished, this, [this, watcher]() {
        const Song song = watcher->result();
        watcher->deleteLater();
        SaveLyricsToFile(song);
      });
      watcher->setFuture(future);
    }
    it = pending_song_saves_.erase(it);
  }

}
