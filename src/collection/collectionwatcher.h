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

  ~CollectionWatcher() override;

  Song::Source source() const { return source_; }

  void set_device_name(const QString &device_name) { device_name_ = device_name; }

  void IncrementalScanAsync();
  void FullScanAsync();
  void SetRescanPausedAsync(const bool pause);
  bool rescan_paused() const { return rescan_paused_; }
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
  void SubdirsDeleted(const CollectionSubdirectoryList &subdirs);
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
    ScanTransaction(CollectionWatcher *watcher, const int dir_id, const bool incremental, const bool ignores_mtime);
    ~ScanTransaction();

    SongList FindSongsInSubdirectory(const QString &path);
    bool HasSongsWithMissingFingerprint(const QString &path);
    bool HasSongsWithMissingLoudnessCharacteristics(const QString &path);
    bool HasSeenSubdir(const QString &path);
    bool HasScannedPath(const QString &path);
    void MarkPathScanned(const QString &path);
    void SetKnownSubdirs(const CollectionSubdirectoryList &subdirs);
    CollectionSubdirectoryList GetImmediateSubdirs(const QString &path);
    CollectionSubdirectoryList GetAllSubdirs();

    void AddToProgress(const quint64 n = 1);
    void AddToProgressMax(const quint64 n);

    // Emits the signals for new & deleted songs etc and clears the lists. This causes the new stuff to be updated on UI.
    void CommitNewOrUpdatedSongs();
    // Logs the deleted, added, changed and restored songs which are committed.
    void LogChanges() const;

    int dir_id() const { return dir_id_; }
    bool is_incremental() const { return incremental_; }
    bool ignores_mtime() const { return ignores_mtime_; }

    SongList deleted_songs;
    SongList readded_songs;
    SongList new_songs;
    SongList touched_songs;
    // What changed for the songs in new_songs which are already in the collection, by song ID, for logging.
    QHash<int, QStringList> song_changes;
    CollectionSubdirectoryList new_subdirs;
    CollectionSubdirectoryList touched_subdirs;
    CollectionSubdirectoryList deleted_subdirs;

    // The entries of the directories, for resolving paths to the names on disk, shared by the whole scan so each directory is only listed once.
    QHash<QString, QStringList> directory_entries_;

   private:
    ScanTransaction &operator=(const ScanTransaction &transaction) { Q_UNUSED(transaction); return *this; }

    CollectionWatcher *watcher_;

    int dir_id_;

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

    int task_id_;

    quint64 progress_max_;
    quint64 progress_;

    QMultiMap<QString, Song> cached_songs_;
    bool cached_songs_dirty_;

    QMultiMap<QString, Song> cached_songs_missing_fingerprint_;
    bool cached_songs_missing_fingerprint_dirty_;

    QMultiMap<QString, Song> cached_songs_missing_loudness_characteristics_;
    bool cached_songs_missing_loudness_characteristics_dirty_;

    CollectionSubdirectoryList known_subdirs_;
    bool known_subdirs_dirty_;
    // Indexes of known_subdirs_, so looking up a subdirectory doesn't go through all of them.
    // known_subdir_paths_ only has the subdirectories with a modification time, known_subdirs_by_parent_ has all of them, keyed by the parent path.
    QSet<QString> known_subdir_paths_;
    QHash<QString, CollectionSubdirectoryList> known_subdirs_by_parent_;

    QSet<QString> scanned_paths_;
  };

 private Q_SLOTS:
  void ReloadSettings();
  void Exit();
  void DirectoryChanged(const QString &subdir);
  void FileSystemWatcherOverflow();
  void IncrementalScanCheck();
  void IncrementalScanNow();
  void FullScanNow();
  void RescanPathsNow();
  void ScanSubdirectory(const CollectionDirectory &dir, const QString &path, const CollectionSubdirectory &subdir, const quint64 files_count, CollectionWatcher::ScanTransaction *t, const bool force_noincremental = false);
  void RescanSongs(const SongList &songs);

 private:
  bool stop_requested() const;
  bool abort_requested() const;
  bool stop_or_abort_requested() const;
  static QHash<QString, SongList> SongsByPath(const SongList &songs);
  // The songs with the exact path, otherwise the songs found by the path in NFC, but not songs of another file on disk whose name only differs in Unicode normalization form.
  static bool FindSongsByPath(const QHash<QString, SongList> &songs_by_path, const QString &path, const QSet<QString> &files_on_disk, SongList &songs_out);
  bool FindSongsByFingerprint(const QString &file, const QString &fingerprint, SongList &songs_out);
  static bool FindSongsByFingerprint(const QString &file, const SongList &songs, const QString &fingerprint, SongList &songs_out);
  inline static QString NoExtensionPart(const QString &fileName);
  inline static QString ExtensionPart(const QString &fileName);
  inline static QString DirectoryPart(const QString &fileName);
  QString PickBestArt(const QStringList &art_automatic_list);
  QUrl ArtForSong(const QString &path, QMap<QString, QStringList> &art_automatic_list);
  static qint64 ArtAutomaticMtime(const QUrl &art_automatic);
  void AddWatch(const CollectionDirectory &dir, const QString &path);
  void RemoveWatch(const CollectionDirectory &dir, const CollectionSubdirectory &subdir);
  void RestoreWatches();
  static quint64 GetMtimeForPath(const QString &path);
  static QString ReadSyncedLyricsFromLrc(const QString &lrc_file);
  static void SetLyricsFromStorage(Song *song, const Song &matching_song, const QString &lrc_file);
  void PerformScan(const bool incremental, const bool ignore_mtimes);

  // Returns the CUE sheets of a directory by the media files they have songs for, without loading the songs.
  // A CUE sheet can have songs for several media files, and it's not necessarily named after them.
  // CUE sheets which can't be read are added to unreadable_cue_sheets, the songs from them are kept until they can be read.
  QHash<QString, QString> FindCueSheets(const QStringList &cue_files, const QString &path, QSet<QString> *unreadable_cue_sheets, QHash<QString, QStringList> *directory_entries) const;
  // Loads the songs of the CUE sheet for the media file, the songs of each CUE sheet are only loaded once in a scan, and only when a media file needs them.
  // Returns false when the CUE sheet can't be read.
  bool LoadCueSongs(const QString &file, const QString &cue_file, const QString &path, QHash<QString, QMap<QString, SongList>> *loaded_cue_sheets, QSet<QString> *unreadable_cue_sheets, QHash<QString, QStringList> *directory_entries, SongList *songs) const;
  // Returns the key for the media files and CUE sheets in the maps, files which only differ in Unicode normalization form are different files.
  static QString PathKey(const QString &filename);
  // Returns the filename with the names of the file and its directories on disk, when it exists with another case or Unicode normalization form, otherwise the filename.
  // The entries of the directories are cached in directory_entries.
  static QString OnDiskFilename(const QString &filename, QHash<QString, QStringList> *directory_entries);
  // Returns the path with every component resolved to the name on disk, for OnDiskFilename().
  static QString OnDiskPath(const QString &path, QHash<QString, QStringList> *directory_entries);
  // Returns the filename on disk with the spelling of the directory being scanned, when it's in that directory.
  static QString ScanPath(const QString &filename_on_disk, const QString &path, QHash<QString, QStringList> *directory_entries);
  // Returns the priority of the CUE sheet for the media file, lower is preferred.
  // A CUE sheet named after the media file with its extension, then in another case on case-insensitive filesystems, then the same without the extension, then other CUE sheets.
  // The entries of the directories are cached in directory_entries.
  static int CueSheetPriority(const QString &media_file, const QString &cue_filename, QHash<QString, QStringList> *directory_entries);

  // Updates the sections of a cue associated and altered (according to mtime) media file during a scan.
  void UpdateCueAssociatedSongs(const QString &fingerprint, const SongList &cue_songs, const QUrl &art_automatic, const SongList &old_cue_songs, ScanTransaction *t) const;
  // Updates the songs of the media file in the collection with the CUE songs of the CUE sheet, and retires the songs which are not used anymore.
  void UpdateCueSongs(const QString &fingerprint, const SongList &cue_songs, const QUrl &art_automatic, const QString &cue_file, const SongList &matching_songs, ScanTransaction *t) const;
  // Marks the CUE songs of a media file unavailable while its CUE sheet can't be read, and adds or updates a song for the whole media file.
  void UseMediaFileWhileCueSheetUnreadable(const QString &file, const QFileInfo &fileinfo, const SongList &cue_songs_in_db, const SongList &file_songs_in_db, const QUrl &art_automatic, const QString &lrc_file, ScanTransaction *t);
  // Updates a single non-cue associated and altered (according to mtime) song during a scan.
  bool UpdateNonCueAssociatedSong(const QString &file, const QString &fingerprint, const SongList &matching_songs, const QUrl &art_automatic, const QString &lrc_file, const bool cue_deleted, ScanTransaction *t);
  // Scans a single media file that's present on the disk but not yet in the collection.
  // It may result in a multiple files added to the collection when the media file has many sections (like a CUE related media file).
  SongList ScanNewFile(const QString &file, const QString &fingerprint, const SongList &cue_songs, const QString &lrc_file) const;

  static void AddChangedSong(const Song &matching_song, const Song &new_song, ScanTransaction *t);

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
  // Set when the file system watcher lost changes, all directories are scanned after rescanning the changed paths.
  bool overflow_scan_pending_;

  int total_watches_;

  CueParser *cue_parser_;

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
