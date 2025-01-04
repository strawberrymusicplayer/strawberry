/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QMultiMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QMutex>

#include "collectiondirectory.h"
#include "includes/shared_ptr.h"
#include "core/song.h"

class QThread;
class QTimer;

class TaskManager;
class TagReaderClient;
class CollectionBackend;
class FileSystemWatcherInterface;
class CueParser;

class CollectionWatcher : public QObject {
  Q_OBJECT

 public:
  explicit CollectionWatcher(const Song::Source source,
                             const SharedPtr<TaskManager> task_manager,
                             const SharedPtr<TagReaderClient> tagreader_client,
                             const SharedPtr<CollectionBackend> backend,
                             QObject *parent = nullptr);

  ~CollectionWatcher();

  Song::Source source() const { return source_; }

  void set_device_name(const QString &device_name) { device_name_ = device_name; }

  void IncrementalScanAsync();
  void FullScanAsync();
  void SetRescanPausedAsync(const bool pause);
  void ReloadSettingsAsync();

  void Stop();
  void CancelStop();
  void Abort();

  void ExitAsync();

  void RescanSongsAsync(const SongList &songs);

 Q_SIGNALS:
  void NewOrUpdatedSongs(const SongList &songs);
  void SongsMTimeUpdated(const SongList &songs);
  void SongsDeleted(const SongList &songs);
  void SongsUnavailable(const SongList &songs, const bool unavailable = true);
  void SongsReadded(const SongList &songs, const bool unavailable = false);
  void SubdirsDiscovered(const CollectionSubdirectoryList &subdirs);
  void SubdirsMTimeUpdated(const CollectionSubdirectoryList &subdirs);
  void CompilationsNeedUpdating();
  void UpdateLastSeen(const int directory_id, const int expire_unavailable_songs_days);
  void ExitFinished();

  void ScanStarted(const int task_id);

 public Q_SLOTS:
  void AddDirectory(const CollectionDirectory &dir, const CollectionSubdirectoryList &subdirs);
  void RemoveDirectory(const CollectionDirectory &dir);
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
    bool HasSongsWithMissingFingerprint(const QString &path);
    bool HasSongsWithMissingLoudnessCharacteristics(const QString &path);
    bool HasSeenSubdir(const QString &path);
    void SetKnownSubdirs(const CollectionSubdirectoryList &subdirs);
    CollectionSubdirectoryList GetImmediateSubdirs(const QString &path);
    CollectionSubdirectoryList GetAllSubdirs();

    void AddToProgress(const quint64 n = 1);
    void AddToProgressMax(const quint64 n);

    // Emits the signals for new & deleted songs etc and clears the lists. This causes the new stuff to be updated on UI.
    void CommitNewOrUpdatedSongs();

    int dir() const { return dir_; }
    bool is_incremental() const { return incremental_; }
    bool ignores_mtime() const { return ignores_mtime_; }

    SongList deleted_songs;
    SongList readded_songs;
    SongList new_songs;
    SongList touched_songs;
    CollectionSubdirectoryList new_subdirs;
    CollectionSubdirectoryList touched_subdirs;
    CollectionSubdirectoryList deleted_subdirs;

    QStringList files_changed_path_;

   private:
    ScanTransaction &operator=(const ScanTransaction&) { return *this; }

    int task_id_;
    quint64 progress_;
    quint64 progress_max_;

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
    int expire_unavailable_songs_days_;

    CollectionWatcher *watcher_;

    QMultiMap<QString, Song> cached_songs_;
    bool cached_songs_dirty_;

    QMultiMap<QString, Song> cached_songs_missing_fingerprint_;
    bool cached_songs_missing_fingerprint_dirty_;

    QMultiMap<QString, Song> cached_songs_missing_loudness_characteristics_;
    bool cached_songs_missing_loudness_characteristics_dirty_;

    CollectionSubdirectoryList known_subdirs_;
    bool known_subdirs_dirty_;
  };

 private Q_SLOTS:
  void ReloadSettings();
  void Exit();
  void DirectoryChanged(const QString &subdir);
  void IncrementalScanCheck();
  void IncrementalScanNow();
  void FullScanNow();
  void RescanPathsNow();
  void ScanSubdirectory(const QString &path, const CollectionSubdirectory &subdir, const quint64 files_count, CollectionWatcher::ScanTransaction *t, const bool force_noincremental = false);
  void RescanSongs(const SongList &songs);

 private:
  bool stop_requested() const;
  bool abort_requested() const;
  bool stop_or_abort_requested() const;
  static bool FindSongsByPath(const SongList &songs, const QString &path, SongList *out);
  bool FindSongsByFingerprint(const QString &file, const QString &fingerprint, SongList *out);
  static bool FindSongsByFingerprint(const QString &file, const SongList &songs, const QString &fingerprint, SongList *out);
  inline static QString NoExtensionPart(const QString &fileName);
  inline static QString ExtensionPart(const QString &fileName);
  inline static QString DirectoryPart(const QString &fileName);
  QString PickBestArt(const QStringList &art_automatic_list);
  QUrl ArtForSong(const QString &path, QMap<QString, QStringList> &art_automatic_list);
  void AddWatch(const CollectionDirectory &dir, const QString &path);
  void RemoveWatch(const CollectionDirectory &dir, const CollectionSubdirectory &subdir);
  static quint64 GetMtimeForCue(const QString &cue_path);
  void PerformScan(const bool incremental, const bool ignore_mtimes);

  // Updates the sections of a cue associated and altered (according to mtime) media file during a scan.
  void UpdateCueAssociatedSongs(const QString &file, const QString &path, const QString &fingerprint, const QString &matching_cue, const QUrl &art_automatic, const SongList &old_cue_songs, ScanTransaction *t) const;
  // Updates a single non-cue associated and altered (according to mtime) song during a scan.
  void UpdateNonCueAssociatedSong(const QString &file, const QString &fingerprint, const SongList &matching_songs, const QUrl &art_automatic, const bool cue_deleted, ScanTransaction *t);
  // Scans a single media file that's present on the disk but not yet in the collection.
  // It may result in a multiple files added to the collection when the media file has many sections (like a CUE related media file).
  SongList ScanNewFile(const QString &file, const QString &path, const QString &fingerprint, const QString &matching_cue, QSet<QString> *cues_processed) const;

  static void AddChangedSong(const QString &file, const Song &matching_song, const Song &new_song, ScanTransaction *t);

  void PerformEBUR128Analysis(Song &song) const;

  quint64 FilesCountForPath(ScanTransaction *t, const QString &path);
  quint64 FilesCountForSubdirs(ScanTransaction *t, const CollectionSubdirectoryList &subdirs, QMap<QString, quint64> &subdir_files_count);

  QString FindCueFilename(const QString &filename);

 private:
  const Song::Source source_;

  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackend> backend_;

  QString device_name_;

  FileSystemWatcherInterface *fs_watcher_;
  QThread *original_thread_;
  QHash<QString, CollectionDirectory> subdir_mapping_;

  // A list of words use to try to identify the (likely) best album cover art found in an directory to use as cover artwork.
  // e.g. using ["front", "cover"] would identify front.jpg and exclude back.jpg.
  QStringList best_art_filters_;

  bool scan_on_startup_;
  bool monitor_;
  bool song_tracking_;
  bool song_ebur128_loudness_analysis_;
  bool mark_songs_unavailable_;
  int expire_unavailable_songs_days_;
  bool overwrite_playcount_;
  bool overwrite_rating_;

  mutable QMutex mutex_stop_;
  bool stop_requested_;

  mutable QMutex mutex_abort_;
  bool abort_requested_;

  QMap<int, CollectionDirectory> watched_dirs_;
  QTimer *rescan_timer_;
  QTimer *periodic_scan_timer_;
  QMap<int, QStringList> rescan_queue_;  // dir id -> list of subdirs to be scanned
  bool rescan_paused_;

  int total_watches_;

  CueParser *cue_parser_;

  static QStringList sValidImages;

  qint64 last_scan_time_;

};

inline QString CollectionWatcher::NoExtensionPart(const QString &fileName) {
  return fileName.contains(u'.') ? fileName.section(u'.', 0, -2) : QLatin1String("");
}
// Thanks Amarok
inline QString CollectionWatcher::ExtensionPart(const QString &fileName) {
  return fileName.contains(u'.') ? fileName.mid(fileName.lastIndexOf(u'.') + 1).toLower() : QLatin1String("");
}
inline QString CollectionWatcher::DirectoryPart(const QString &fileName) {
  return fileName.section(u'/', 0, -2);
}

#endif  // COLLECTIONWATCHER_H
