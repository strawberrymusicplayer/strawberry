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

#ifndef COLLECTIONWATCHER_H
#define COLLECTIONWATCHER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "directory.h"
#include "core/song.h"

class QThread;
class QTimer;

class CollectionBackend;
class FileSystemWatcherInterface;
class TaskManager;
class CueParser;

class CollectionWatcher : public QObject {
  Q_OBJECT

 public:
  explicit CollectionWatcher(Song::Source source, QObject *parent = nullptr);

  void set_backend(CollectionBackend *backend) { backend_ = backend; }
  void set_task_manager(TaskManager *task_manager) { task_manager_ = task_manager; }
  void set_device_name(const QString& device_name) { device_name_ = device_name; }

  void IncrementalScanAsync();
  void FullScanAsync();
  void RescanTracksAsync(const SongList &songs);
  void SetRescanPausedAsync(bool pause);
  void ReloadSettingsAsync();

  void Stop() { stop_requested_ = true; }

  void ExitAsync();

 signals:
  void NewOrUpdatedSongs(const SongList &songs);
  void SongsMTimeUpdated(const SongList &songs);
  void SongsDeleted(const SongList &songs);
  void SongsUnavailable(const SongList &songs);
  void SongsReadded(const SongList &songs, bool unavailable = false);
  void SubdirsDiscovered(const SubdirectoryList &subdirs);
  void SubdirsMTimeUpdated(const SubdirectoryList &subdirs);
  void CompilationsNeedUpdating();
  void ExitFinished();

  void ScanStarted(int task_id);

 public slots:
  void ReloadSettings();
  void AddDirectory(const Directory &dir, const SubdirectoryList &subdirs);
  void RemoveDirectory(const Directory &dir);
  void SetRescanPaused(bool pause);

 private:
  // This class encapsulates a full or partial scan of a directory.
  // Each directory has one or more subdirectories, and any number of subdirectories can be scanned during one transaction.
  // ScanSubdirectory() adds its results to the members of this transaction class,
  // and they are "committed" through calls to the CollectionBackend in the transaction's dtor.
  // The transaction also caches the list of songs in this directory according to the collection.
  // Multiple calls to FindSongsInSubdirectory during one transaction will only result in one call to CollectionBackend::FindSongsInDirectory.
  class ScanTransaction {
   public:
    ScanTransaction(CollectionWatcher *watcher, const int dir, const bool incremental, const bool ignores_mtime, const bool mark_songs_unavailable);
    ~ScanTransaction();

    SongList FindSongsInSubdirectory(const QString &path);
    bool HasSeenSubdir(const QString &path);
    void SetKnownSubdirs(const SubdirectoryList &subdirs);
    SubdirectoryList GetImmediateSubdirs(const QString &path);
    SubdirectoryList GetAllSubdirs();

    void AddToProgress(int n = 1);
    void AddToProgressMax(int n);

    // Emits the signals for new & deleted songs etc and clears the lists. This causes the new stuff to be updated on UI.
    void CommitNewOrUpdatedSongs();

    int dir() const { return dir_; }
    bool is_incremental() const { return incremental_; }
    bool ignores_mtime() const { return ignores_mtime_; }

    SongList deleted_songs;
    SongList readded_songs;
    SongList new_songs;
    SongList touched_songs;
    SubdirectoryList new_subdirs;
    SubdirectoryList touched_subdirs;
    SubdirectoryList deleted_subdirs;

   private:
    ScanTransaction(const ScanTransaction&) {}
    ScanTransaction& operator=(const ScanTransaction&) { return *this; }

    int task_id_;
    int progress_;
    int progress_max_;

    int dir_;
    // Incremental scan enters a directory only if it has changed since the last scan.
    bool incremental_;
    // This type of scan updates every file in a folder that's being scanned.
    // Even if it detects the file hasn't changed since the last scan.
    // Also, since it's ignoring mtimes on folders too, it will go as deep in the folder hierarchy as it's possible.
    bool ignores_mtime_;

    // Set this to true to prevent deleting missing files from database.
    // Useful for unstable network connections.
    bool mark_songs_unavailable_;

    CollectionWatcher *watcher_;

    SongList cached_songs_;
    bool cached_songs_dirty_;

    SubdirectoryList known_subdirs_;
    bool known_subdirs_dirty_;
  };

 private slots:
  void Exit();
  void DirectoryChanged(const QString &subdir);
  void IncrementalScanNow();
  void FullScanNow();
  void RescanTracksNow();
  void RescanPathsNow();
  void ScanSubdirectory(const QString &path, const Subdirectory &subdir, ScanTransaction *t, bool force_noincremental = false);

 private:
  static bool FindSongByPath(const SongList &list, const QString &path, Song *out);
  inline static QString NoExtensionPart(const QString &fileName);
  inline static QString ExtensionPart(const QString &fileName);
  inline static QString DirectoryPart(const QString &fileName);
  QString PickBestImage(const QStringList &images);
  QUrl ImageForSong(const QString &path, QMap<QString, QStringList> &album_art);
  void AddWatch(const Directory &dir, const QString &path);
  void RemoveWatch(const Directory &dir, const Subdirectory &subdir);
  quint64 GetMtimeForCue(const QString &cue_path);
  void PerformScan(bool incremental, bool ignore_mtimes);

  // Updates the sections of a cue associated and altered (according to mtime) media file during a scan.
  void UpdateCueAssociatedSongs(const QString &file, const QString &path, const QString &matching_cue, const QUrl &image, ScanTransaction *t);
  // Updates a single non-cue associated and altered (according to mtime) song during a scan.
  void UpdateNonCueAssociatedSong(const QString &file, const Song &matching_song, const QUrl &image, bool cue_deleted, ScanTransaction *t);
  // Updates a new song with some metadata taken from it's equivalent old song (for example rating and score).
  void PreserveUserSetData(const QString &file, const QUrl &image, const Song &matching_song, Song *out, ScanTransaction *t);
  // Scans a single media file that's present on the disk but not yet in the collection.
  // It may result in a multiple files added to the collection when the media file has many sections (like a CUE related media file).
  SongList ScanNewFile(const QString &file, const QString &path, const QString &matching_cue, QSet<QString> *cues_processed);

 private:
  Song::Source source_;
  CollectionBackend *backend_;
  TaskManager *task_manager_;
  QString device_name_;

  FileSystemWatcherInterface *fs_watcher_;
  QHash<QString, Directory> subdir_mapping_;

  // A list of words use to try to identify the (likely) best image found in an directory to use as cover artwork.
  // e.g. using ["front", "cover"] would identify front.jpg and exclude back.jpg.
  QStringList best_image_filters_;

  bool scan_on_startup_;
  bool monitor_;
  bool mark_songs_unavailable_;

  bool stop_requested_;
  bool rescan_in_progress_; // True if RescanTracksNow() has been called and is working.

  QMap<int, Directory> watched_dirs_;
  QTimer *rescan_timer_;
  QMap<int, QStringList> rescan_queue_; // dir id -> list of subdirs to be scanned
  bool rescan_paused_;

  int total_watches_;

  CueParser *cue_parser_;

  static QStringList sValidImages;

  SongList song_rescan_queue_; // Set by ui thread

  QThread *original_thread_;

};

inline QString CollectionWatcher::NoExtensionPart(const QString& fileName) {
  return fileName.contains('.') ? fileName.section('.', 0, -2) : "";
}
// Thanks Amarok
inline QString CollectionWatcher::ExtensionPart(const QString& fileName) {
  return fileName.contains( '.' ) ? fileName.mid( fileName.lastIndexOf('.') + 1 ).toLower() : "";
}
inline QString CollectionWatcher::DirectoryPart(const QString& fileName) {
  return fileName.section('/', 0, -2);
}

#endif  // COLLECTIONWATCHER_H

