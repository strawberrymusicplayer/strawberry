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

#ifndef COLLECTIONMANAGER_H
#define COLLECTIONMANAGER_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QHash>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QThread;
class Thread;
class Database;
class TaskManager;
class TagReaderClient;
class CollectionBackend;
class CollectionModel;
class CollectionWatcher;
class AlbumCoverLoader;

class CollectionManager : public QObject {
  Q_OBJECT

 public:
  explicit CollectionManager(const SharedPtr<Database> database,
                             const SharedPtr<TaskManager> task_manager,
                             const SharedPtr<TagReaderClient> tagreader_client,
                             const SharedPtr<AlbumCoverLoader> albumcover_loader,
                             QObject *parent = nullptr);

  ~CollectionManager() override;

  static const char *kSongsTable;
  static const char *kFtsTable;
  static const char *kDirsTable;
  static const char *kSubdirsTable;

  void Init();
  void Exit();

  SharedPtr<CollectionBackend> backend() const { return backend_; }
  CollectionModel *model() const { return model_; }

  QString full_rescan_reason(const int schema_version) const { return full_rescan_revisions_.value(schema_version, QString()); }

  void SyncPlaycountAndRatingToFilesAsync();

 private:
  void SyncPlaycountAndRatingToFiles();
  void SavePendingSongSaves();
  static Song CurrentLyricsToSave(const SharedPtr<CollectionBackend> &backend, const Song &song);
  void SaveLyricsToFile(const Song &song);
  static bool IsCurrentSongSaveDeferred(const Song &song, const QUrl &current_song_url);

 public Q_SLOTS:
  void ReloadSettings();

  void PauseWatcher();
  void ResumeWatcher();

  void FullScan();
  void StopScan();
  void Rescan(const SongList &songs);

  void IncrementalScan();

  void CurrentSongChanged(const Song &song);
  void Stopped();

  // Saves lyrics found automatically for a collection song, to the tags if enabled in the settings, otherwise to the database only.
  void SaveLyrics(const Song &song, const QString &lyrics, const QString &synced_lyrics);

 private Q_SLOTS:
  void ExitReceived();
  void SongsPlaycountChanged(const SongList &songs, const bool save_tags = false);
  void SongsRatingChanged(const SongList &songs, const bool save_tags = false);
  void SongsLyricsChanged(const SongList &songs, const bool save_lyrics_tags, const bool save_synced_lyrics_tags);

 Q_SIGNALS:
  void Error(const QString &error);
  void ExitFinished();

 private:
  class PendingSongSave {
   public:
    Song song;
    bool save_playcount = false;
    bool save_rating = false;
    bool save_lyrics = false;
  };

  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;

  SharedPtr<CollectionBackend> backend_;
  CollectionModel *model_;

  CollectionWatcher *watcher_;
  Thread *watcher_thread_;
  QThread *original_thread_;

  // DB schema versions which should trigger a full collection rescan (each of those with a short reason why).
  QHash<int, QString> full_rescan_revisions_;

  QList<QObject*> wait_for_exit_;

  bool save_playcounts_to_files_;
  bool save_ratings_to_files_;
  bool save_lyrics_to_files_;

  QUrl current_song_url_;

  QMap<QUrl, SharedPtr<PendingSongSave>> pending_song_saves_;
};

#endif
