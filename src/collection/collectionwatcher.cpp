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

#include "config.h"

#include <utility>
#include <chrono>

#include <QObject>
#include <QThread>
#include <QIODevice>
#include <QStorageInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QMutexLocker>
#include <QSettings>

#include "core/filesystemwatcherinterface.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/settings.h"
#include "utilities/imageutils.h"
#include "constants/timeconstants.h"
#include "constants/filesystemconstants.h"
#include "tagreader/tagreaderclient.h"
#include "collectiondirectory.h"
#include "collectionbackend.h"
#include "collectionwatcher.h"
#include "playlistparsers/cueparser.h"
#include "constants/collectionsettings.h"
#include "engine/ebur128measures.h"
#ifdef HAVE_SONGFINGERPRINTING
#  include "engine/chromaprinter.h"
#endif
#ifdef HAVE_EBUR128
#  include "engine/ebur128analysis.h"
#endif

// This is defined by one of the windows headers that is included by taglib.
#ifdef RemoveDirectory
#  undef RemoveDirectory
#endif

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

QStringList CollectionWatcher::sValidImages = QStringList() << u"jpg"_s << u"png"_s << u"gif"_s << u"jpeg"_s;

CollectionWatcher::CollectionWatcher(const Song::Source source,
                                     const SharedPtr<TaskManager> task_manager,
                                     const SharedPtr<TagReaderClient> tagreader_client,
                                     const SharedPtr<CollectionBackend> backend,
                                     QObject *parent)
    : QObject(parent),
      source_(source),
      task_manager_(task_manager),
      tagreader_client_(tagreader_client),
      backend_(backend),
      fs_watcher_(FileSystemWatcherInterface::Create(this)),
      original_thread_(nullptr),
      scan_on_startup_(true),
      monitor_(true),
      song_tracking_(false),
      song_ebur128_loudness_analysis_(false),
      mark_songs_unavailable_(source_ == Song::Source::Collection),
      expire_unavailable_songs_days_(60),
      overwrite_playcount_(false),
      overwrite_rating_(false),
      stop_requested_(false),
      abort_requested_(false),
      rescan_timer_(new QTimer(this)),
      periodic_scan_timer_(new QTimer(this)),
      rescan_paused_(false),
      total_watches_(0),
      cue_parser_(new CueParser(tagreader_client, backend, this)),
      last_scan_time_(0) {

  setObjectName(source_ == Song::Source::Collection ? QLatin1String(QObject::metaObject()->className()) : QStringLiteral("%1%2").arg(Song::DescriptionForSource(source_), QLatin1String(QObject::metaObject()->className())));

  original_thread_ = thread();

  rescan_timer_->setInterval(2s);
  rescan_timer_->setSingleShot(true);

  periodic_scan_timer_->setInterval(86400 * kMsecPerSec);
  periodic_scan_timer_->setSingleShot(false);

  const QStringList image_formats = ImageUtils::SupportedImageFormats();
  for (const QString &format : image_formats) {
    if (!sValidImages.contains(format)) {
      sValidImages.append(format);
    }
  }

  ReloadSettings();

  QObject::connect(fs_watcher_, &FileSystemWatcherInterface::PathChanged, this, &CollectionWatcher::DirectoryChanged, Qt::UniqueConnection);
  QObject::connect(rescan_timer_, &QTimer::timeout, this, &CollectionWatcher::RescanPathsNow);
  QObject::connect(periodic_scan_timer_, &QTimer::timeout, this, &CollectionWatcher::IncrementalScanCheck);

}

CollectionWatcher::~CollectionWatcher() {

  qLog(Debug) << "Collection watcher" << this << "for" << Song::TextForSource(source_) << "deleted.";

}

void CollectionWatcher::ExitAsync() {
  QMetaObject::invokeMethod(this, &CollectionWatcher::Exit, Qt::QueuedConnection);
}

void CollectionWatcher::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());

  Abort();
  if (backend_) backend_->Close();
  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

void CollectionWatcher::Stop() {

  QMutexLocker l(&mutex_stop_);
  stop_requested_ = true;

}

void CollectionWatcher::CancelStop() {

  QMutexLocker l(&mutex_stop_);
  stop_requested_ = false;

}

bool CollectionWatcher::stop_requested() const {

  QMutexLocker l(&mutex_stop_);
  return stop_requested_;

}

void CollectionWatcher::Abort() {

  QMutexLocker l(&mutex_abort_);
  abort_requested_ = true;

}

bool CollectionWatcher::abort_requested() const {

  QMutexLocker l(&mutex_abort_);
  return abort_requested_;

}

bool CollectionWatcher::stop_or_abort_requested() const {

  return stop_requested() || abort_requested();

}

void CollectionWatcher::ReloadSettingsAsync() {

  QMetaObject::invokeMethod(this, &CollectionWatcher::ReloadSettings, Qt::QueuedConnection);

}

void CollectionWatcher::ReloadSettings() {

  const bool was_monitoring_before = monitor_;
  Settings s;
  s.beginGroup(CollectionSettings::kSettingsGroup);
  if (source_ == Song::Source::Collection) {
    scan_on_startup_ = s.value(CollectionSettings::kStartupScan, true).toBool();
    monitor_ = s.value(CollectionSettings::kMonitor, true).toBool();
  }
  else {
    scan_on_startup_ = true;
    monitor_ = true;
  }
  const QStringList filters = s.value(CollectionSettings::kCoverArtPatterns, QStringList() << u"front"_s << u"cover"_s).toStringList();
  if (source_ == Song::Source::Collection) {
    song_tracking_ = s.value(CollectionSettings::kSongTracking, false).toBool();
    song_ebur128_loudness_analysis_ = s.value(CollectionSettings::kSongENUR128LoudnessAnalysis, false).toBool();
    mark_songs_unavailable_ = song_tracking_ ? true : s.value(CollectionSettings::kMarkSongsUnavailable, true).toBool();
  }
  else {
    song_tracking_ = false;
    song_ebur128_loudness_analysis_ = false;
    mark_songs_unavailable_ = false;
  }
  expire_unavailable_songs_days_ = s.value(CollectionSettings::kExpireUnavailableSongs, 60).toInt();
  overwrite_playcount_ = s.value(CollectionSettings::kOverwritePlaycount, false).toBool();
  overwrite_rating_ = s.value(CollectionSettings::kOverwriteRating, false).toBool();
  s.endGroup();

  best_art_filters_.clear();
  for (const QString &filter : filters) {
    QString str = filter.trimmed();
    if (!str.isEmpty()) best_art_filters_ << str;
  }

  if (!monitor_ && was_monitoring_before) {
    fs_watcher_->Clear();
  }
  else if (monitor_ && !was_monitoring_before) {
    // Add all directories to all QFileSystemWatchers again
    for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
      const CollectionSubdirectoryList subdirs = backend_->SubdirsInDirectory(dir.id);
      for (const CollectionSubdirectory &subdir : subdirs) {
        AddWatch(dir, subdir.path);
      }
    }
  }

  if (source_ == Song::Source::Collection) {
    if (monitor_ && scan_on_startup_ && mark_songs_unavailable_ && !periodic_scan_timer_->isActive()) {
      periodic_scan_timer_->start();
    }
    else if ((!monitor_ || !scan_on_startup_ || !mark_songs_unavailable_) && periodic_scan_timer_->isActive()) {
      periodic_scan_timer_->stop();
    }
  }

}

CollectionWatcher::ScanTransaction::ScanTransaction(CollectionWatcher *watcher, const int dir, const bool incremental, const bool ignores_mtime, const bool mark_songs_unavailable)
    : progress_(0),
      progress_max_(0),
      dir_(dir),
      incremental_(incremental),
      ignores_mtime_(ignores_mtime),
      mark_songs_unavailable_(mark_songs_unavailable),
      expire_unavailable_songs_days_(60),
      watcher_(watcher),
      cached_songs_dirty_(true),
      cached_songs_missing_fingerprint_dirty_(true),
      cached_songs_missing_loudness_characteristics_dirty_(true),
      known_subdirs_dirty_(true) {

  QString description;

  if (watcher_->device_name_.isEmpty()) {
    description = tr("Updating collection");
  }
  else {
    description = tr("Updating %1").arg(watcher_->device_name_);
  }

  task_id_ = watcher_->task_manager_->StartTask(description);
  Q_EMIT watcher_->ScanStarted(task_id_);

}

CollectionWatcher::ScanTransaction::~ScanTransaction() {

  // If we're stopping then don't commit the transaction
  if (!watcher_->stop_or_abort_requested()) {
    CommitNewOrUpdatedSongs();
  }

  watcher_->task_manager_->SetTaskFinished(task_id_);

}

void CollectionWatcher::ScanTransaction::AddToProgress(const quint64 n) {

  progress_ += n;
  watcher_->task_manager_->SetTaskProgress(task_id_, progress_, progress_max_);

}

void CollectionWatcher::ScanTransaction::AddToProgressMax(const quint64 n) {

  progress_max_ += n;
  watcher_->task_manager_->SetTaskProgress(task_id_, progress_, progress_max_);

}

void CollectionWatcher::ScanTransaction::CommitNewOrUpdatedSongs() {

  if (!deleted_songs.isEmpty()) {
    if (mark_songs_unavailable_ && watcher_->source() == Song::Source::Collection) {
      Q_EMIT watcher_->SongsUnavailable(deleted_songs);
    }
    else {
      Q_EMIT watcher_->SongsDeleted(deleted_songs);
    }
    deleted_songs.clear();
  }

  if (!new_songs.isEmpty()) {
    Q_EMIT watcher_->NewOrUpdatedSongs(new_songs);
    new_songs.clear();
  }

  if (!touched_songs.isEmpty()) {
    Q_EMIT watcher_->SongsMTimeUpdated(touched_songs);
    touched_songs.clear();
  }

  if (!readded_songs.isEmpty()) {
    Q_EMIT watcher_->SongsReadded(readded_songs);
    readded_songs.clear();
  }

  if (!new_subdirs.isEmpty()) {
    Q_EMIT watcher_->SubdirsDiscovered(new_subdirs);
  }

  if (!touched_subdirs.isEmpty()) {
    Q_EMIT watcher_->SubdirsMTimeUpdated(touched_subdirs);
    touched_subdirs.clear();
  }

  for (const CollectionSubdirectory &subdir : std::as_const(deleted_subdirs)) {
    if (watcher_->watched_dirs_.contains(dir_)) {
      watcher_->RemoveWatch(watcher_->watched_dirs_[dir_], subdir);
    }
  }
  deleted_subdirs.clear();

  if (watcher_->monitor_) {
    // Watch the new subdirectories
    for (const CollectionSubdirectory &subdir : std::as_const(new_subdirs)) {
      if (watcher_->watched_dirs_.contains(dir_)) {
        watcher_->AddWatch(watcher_->watched_dirs_[dir_], subdir.path);
      }
    }
  }
  new_subdirs.clear();

  if (incremental_ || ignores_mtime_) {
    Q_EMIT watcher_->UpdateLastSeen(dir_, expire_unavailable_songs_days_);
  }

}


SongList CollectionWatcher::ScanTransaction::FindSongsInSubdirectory(const QString &path) {

  if (cached_songs_dirty_) {
    const SongList songs = watcher_->backend_->FindSongsInDirectory(dir_);
    for (const Song &song : songs) {
      const QString p = song.url().toLocalFile().section(u'/', 0, -2);
      cached_songs_.insert(p, song);
    }
    cached_songs_dirty_ = false;
  }

  if (cached_songs_.contains(path)) {
    return cached_songs_.values(path);
  }

  return SongList();

}

bool CollectionWatcher::ScanTransaction::HasSongsWithMissingFingerprint(const QString &path) {

  if (cached_songs_missing_fingerprint_dirty_) {
    const SongList songs = watcher_->backend_->SongsWithMissingFingerprint(dir_);
    for (const Song &song : songs) {
      const QString p = song.url().toLocalFile().section(u'/', 0, -2);
      cached_songs_missing_fingerprint_.insert(p, song);
    }
    cached_songs_missing_fingerprint_dirty_ = false;
  }

  return cached_songs_missing_fingerprint_.contains(path);

}

bool CollectionWatcher::ScanTransaction::HasSongsWithMissingLoudnessCharacteristics(const QString &path) {

  if (cached_songs_missing_loudness_characteristics_dirty_) {
    const SongList songs = watcher_->backend_->SongsWithMissingLoudnessCharacteristics(dir_);
    for (const Song &song : songs) {
      const QString p = song.url().toLocalFile().section(u'/', 0, -2);
      cached_songs_missing_loudness_characteristics_.insert(p, song);
    }
    cached_songs_missing_loudness_characteristics_dirty_ = false;
  }

  return cached_songs_missing_loudness_characteristics_.contains(path);

}

void CollectionWatcher::ScanTransaction::SetKnownSubdirs(const CollectionSubdirectoryList &subdirs) {

  known_subdirs_ = subdirs;
  known_subdirs_dirty_ = false;

}

bool CollectionWatcher::ScanTransaction::HasSeenSubdir(const QString &path) {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_));
  }

  return std::any_of(known_subdirs_.begin(), known_subdirs_.end(), [path](const CollectionSubdirectory &subdir) { return subdir.path == path && subdir.mtime != 0; });

}

CollectionSubdirectoryList CollectionWatcher::ScanTransaction::GetImmediateSubdirs(const QString &path) {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_));
  }

  CollectionSubdirectoryList ret;
  for (const CollectionSubdirectory &subdir : std::as_const(known_subdirs_)) {
    if (subdir.path.left(subdir.path.lastIndexOf(QDir::separator())) == path && subdir.mtime != 0) {
      ret << subdir;
    }
  }

  return ret;

}

CollectionSubdirectoryList CollectionWatcher::ScanTransaction::GetAllSubdirs() {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_));
  }

  return known_subdirs_;

}

void CollectionWatcher::AddDirectory(const CollectionDirectory &dir, const CollectionSubdirectoryList &subdirs) {

  {
    const QFileInfo path_info(dir.path);
    if (path_info.isSymbolicLink()) {
      const QStorageInfo storage_info(path_info.symLinkTarget());
      if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
        qLog(Warning) << "Ignoring collection directory path" << dir.path << "which is a symbolic link to path" << path_info.symLinkTarget() << "with rejected filesystem type" << storage_info.fileSystemType();
        return;
      }
    }
    else {
      const QStorageInfo storage_info(dir.path);
      if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
        qLog(Warning) << "Ignoring collection directory path" << dir.path << "with rejected filesystem type" << storage_info.fileSystemType();
        return;
      }
    }
  }

  CancelStop();

  watched_dirs_[dir.id] = dir;

  if (subdirs.isEmpty()) {
    // This is a new directory that we've never seen before. Scan it fully.
    ScanTransaction transaction(this, dir.id, false, false, mark_songs_unavailable_);
    const quint64 files_count = FilesCountForPath(&transaction, dir.path);
    transaction.SetKnownSubdirs(subdirs);
    transaction.AddToProgressMax(files_count);
    ScanSubdirectory(dir.path, CollectionSubdirectory(), files_count, &transaction);
    last_scan_time_ = QDateTime::currentSecsSinceEpoch();
  }
  else {
    if (monitor_) {
      for (const CollectionSubdirectory &subdir : subdirs) {
        AddWatch(dir, subdir.path);
      }
    }
    if (scan_on_startup_) {
      // We can do an incremental scan - looking at the mtimes of each subdirectory and only rescan if the directory has changed.
      ScanTransaction transaction(this, dir.id, true, false, mark_songs_unavailable_);
      QMap<QString, quint64> subdir_files_count;
      const quint64 files_count = FilesCountForSubdirs(&transaction, subdirs, subdir_files_count);
      transaction.SetKnownSubdirs(subdirs);
      transaction.AddToProgressMax(files_count);
      for (const CollectionSubdirectory &subdir : subdirs) {
        if (stop_or_abort_requested()) break;
        ScanSubdirectory(subdir.path, subdir, subdir_files_count[subdir.path], &transaction);
      }
      if (!stop_or_abort_requested()) {
        last_scan_time_ = QDateTime::currentSecsSinceEpoch();
      }
    }
  }

  Q_EMIT CompilationsNeedUpdating();

}

void CollectionWatcher::ScanSubdirectory(const QString &path, const CollectionSubdirectory &subdir, const quint64 files_count, ScanTransaction *t, const bool force_noincremental) {

  const QFileInfo path_info(path);

  if (path_info.isSymLink()) {
    const QString real_path = path_info.symLinkTarget();
    const QStorageInfo storage_info(real_path);
    if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
      qLog(Warning) << "Ignoring symbolic link" << path << "which links to" << real_path << "with rejected filesystem type" << storage_info.fileSystemType();
      return;
    }
    // Do not scan symlinked dirs that are already in collection
    for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
      if (real_path.startsWith(dir.path)) {
        return;
      }
    }
  }
  else {
    const QStorageInfo storage_info(path);
    if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
      qLog(Warning) << "Ignoring path" << path << "with rejected filesystem type" << storage_info.fileSystemType();
      return;
    }
  }

  bool songs_missing_fingerprint = false;
  bool songs_missing_loudness_characteristics = false;
#ifdef HAVE_SONGFINGERPRINTING
  if (song_tracking_) {
    songs_missing_fingerprint = t->HasSongsWithMissingFingerprint(path);
  }
#endif
#ifdef HAVE_EBUR128
  if (song_ebur128_loudness_analysis_) {
    songs_missing_loudness_characteristics = t->HasSongsWithMissingLoudnessCharacteristics(path);
  }
#endif

  if (!t->ignores_mtime() && !force_noincremental && t->is_incremental() && subdir.mtime == path_info.lastModified().toSecsSinceEpoch() && !songs_missing_fingerprint && !songs_missing_loudness_characteristics) {
    // The directory hasn't changed since last time
    t->AddToProgress(files_count);
    return;
  }

  QMap<QString, QStringList> album_art;
  QStringList files_on_disk;
  CollectionSubdirectoryList my_new_subdirs;

  // If a directory is moved then only its parent gets a changed notification, so we need to look and see if any of our children don't exist anymore.
  // If one has been removed, "rescan" it to get the deleted songs
  const CollectionSubdirectoryList previous_subdirs = t->GetImmediateSubdirs(path);
  for (const CollectionSubdirectory &prev_subdir : previous_subdirs) {
    if (!QFile::exists(prev_subdir.path) && prev_subdir.path != path) {
      ScanSubdirectory(prev_subdir.path, prev_subdir, 0, t, true);
    }
  }

  // First we "quickly" get a list of the files in the directory that we think might be music.  While we're here, we also look for new subdirectories and possible album artwork.
  QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  while (it.hasNext()) {

    if (stop_or_abort_requested()) return;

    const QString child_filepath = it.next();
    const QFileInfo child_fileinfo(child_filepath);

    if (child_fileinfo.isSymLink()) {
      QStorageInfo storage_info(child_fileinfo.symLinkTarget());
      if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
        qLog(Warning) << "Ignoring symbolic link" << child_filepath << "which links to" << child_fileinfo.symLinkTarget() << "with rejected filesystem type" << storage_info.fileSystemType();
        continue;
      }
    }

    if (child_fileinfo.isDir()) {
      if (!t->HasSeenSubdir(child_filepath)) {
        // We haven't seen this subdirectory before - add it to a list, and later we'll tell the backend about it and scan it.
        CollectionSubdirectory new_subdir;
        new_subdir.directory_id = -1;
        new_subdir.path = child_filepath;
        new_subdir.mtime = child_fileinfo.lastModified().toSecsSinceEpoch();
        my_new_subdirs << new_subdir;
      }
      t->AddToProgress(1);
    }
    else {
      QString ext_part(ExtensionPart(child_filepath));
      QString dir_part(DirectoryPart(child_filepath));
      if (Song::kRejectedExtensions.contains(child_fileinfo.suffix(), Qt::CaseInsensitive) || child_fileinfo.baseName() == "qt_temp"_L1) {
        t->AddToProgress(1);
      }
      else if (sValidImages.contains(ext_part)) {
        album_art[dir_part] << child_filepath;
        t->AddToProgress(1);
      }
      else if (tagreader_client_->IsMediaFileBlocking(child_filepath)) {
        files_on_disk << child_filepath;
      }
      else {
        t->AddToProgress(1);
      }
    }
  }

  if (stop_or_abort_requested()) return;

  // Ask the database for a list of files in this directory
  SongList songs_in_db = t->FindSongsInSubdirectory(path);

  QSet<QString> cues_processed;

  // Now compare the list from the database with the list of files on disk
  QStringList files_on_disk_copy = files_on_disk;
  for (const QString &file : files_on_disk_copy) {

    if (stop_or_abort_requested()) return;

    // Associated CUE
    QString new_cue = CueParser::FindCueFilename(file);

    SongList matching_songs;
    if (FindSongsByPath(songs_in_db, file, &matching_songs)) {  // Found matching song in DB by path.

      Song matching_song = matching_songs.first();

      // The song is in the database and still on disk.
      // Check the mtime to see if it's been changed since it was added.
      QFileInfo fileinfo(file);

      if (!fileinfo.exists()) {
        // Partially fixes race condition - if file was removed between being added to the list and now.
        files_on_disk.removeAll(file);
        t->AddToProgress(1);
        continue;
      }

      // CUE sheet's path from collection (if any).
      qint64 matching_song_cue_mtime = static_cast<qint64>(GetMtimeForCue(matching_song.cue_path()));

      // CUE sheet's path from this file (if any).
      qint64 new_cue_mtime = 0;
      if (!new_cue.isEmpty()) {
        new_cue_mtime = static_cast<qint64>(GetMtimeForCue(new_cue));
      }

      const bool cue_added = new_cue_mtime != 0 && !matching_song.has_cue();
      const bool cue_changed = new_cue_mtime != 0 && matching_song.has_cue() && new_cue != matching_song.cue_path();
      const bool cue_deleted = matching_song.has_cue() && new_cue_mtime == 0;

      // Watch out for CUE songs which have their mtime equal to qMax(media_file_mtime, cue_sheet_mtime)
      bool changed = (matching_song.mtime() != qMax(fileinfo.lastModified().toSecsSinceEpoch(), matching_song_cue_mtime)) || cue_deleted || cue_added || cue_changed;

      // Also want to look to see whether the album art has changed
      const QUrl art_automatic = ArtForSong(file, album_art);
      if (matching_song.art_automatic() != art_automatic || (!matching_song.art_automatic().isEmpty() && !matching_song.art_automatic_is_valid())) {
        changed = true;
      }

      bool missing_fingerprint = false;
      bool missing_loudness_characteristics = false;
#ifdef HAVE_SONGFINGERPRINTING
      if (song_tracking_ && matching_song.fingerprint().isEmpty()) {
        missing_fingerprint = true;
      }
#endif
#ifdef HAVE_EBUR128
      if (song_ebur128_loudness_analysis_ && (!matching_song.ebur128_integrated_loudness_lufs() || !matching_song.ebur128_loudness_range_lu())) {
        missing_loudness_characteristics = true;
      }
#endif

      if (changed) {
        qLog(Debug) << file << "has changed.";
      }
      else if (missing_fingerprint) {
        qLog(Debug) << file << "is missing fingerprint.";
      }
      else if (missing_loudness_characteristics) {
        qLog(Debug) << file << "is missing EBU R 128 loudness characteristics.";
      }

      // The song's changed or missing fingerprint - create fingerprint and reread the metadata from file.
      if (t->ignores_mtime() || changed || missing_fingerprint || missing_loudness_characteristics) {

        QString fingerprint;
#ifdef HAVE_SONGFINGERPRINTING
        if (song_tracking_) {
          Chromaprinter chromaprinter(file);
          fingerprint = chromaprinter.CreateFingerprint();
          if (fingerprint.isEmpty()) {
            fingerprint = "NONE"_L1;
          }
        }
#endif

        if (new_cue.isEmpty() || new_cue_mtime == 0) {  // If no CUE or it's about to lose it.
          UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, cue_deleted, t);
        }
        else {  // If CUE associated.
          UpdateCueAssociatedSongs(file, path, fingerprint, new_cue, art_automatic, matching_songs, t);
        }
      }

      // Nothing has changed - mark the song available without re-scanning
      else if (matching_song.unavailable()) {
        qLog(Debug) << "Unavailable song" << file << "restored.";
        t->readded_songs << matching_songs;
      }

    }
    else {  // Search the DB by fingerprint.
      QString fingerprint;
#ifdef HAVE_SONGFINGERPRINTING
      if (song_tracking_) {
        Chromaprinter chromaprinter(file);
        fingerprint = chromaprinter.CreateFingerprint();
        if (fingerprint.isEmpty()) {
          fingerprint = "NONE"_L1;
        }
      }
#endif
      if (song_tracking_ && !fingerprint.isEmpty() && fingerprint != "NONE"_L1 && FindSongsByFingerprint(file, fingerprint, &matching_songs)) {

        // The song is in the database and still on disk.
        // Check the mtime to see if it's been changed since it was added.
        QFileInfo fileinfo(file);
        if (!fileinfo.exists()) {
          // Partially fixes race condition - if file was removed between being added to the list and now.
          files_on_disk.removeAll(file);
          t->AddToProgress(1);
          continue;
        }

        // Make sure the songs aren't deleted, as they still exist elsewhere with a different file path.
        bool matching_songs_has_cue = false;
        for (const Song &matching_song : std::as_const(matching_songs)) {
          QString matching_filename = matching_song.url().toLocalFile();
          if (!t->files_changed_path_.contains(matching_filename)) {
            t->files_changed_path_ << matching_filename;
            qLog(Debug) << matching_filename << "has changed path to" << file;
          }
          if (t->deleted_songs.contains(matching_song)) {
            t->deleted_songs.removeAll(matching_song);
          }
          if (matching_song.has_cue()) {
            matching_songs_has_cue = true;
          }
        }

        // CUE sheet's path from this file (if any).
        qint64 new_cue_mtime = 0;
        if (!new_cue.isEmpty()) {
          new_cue_mtime = static_cast<qint64>(GetMtimeForCue(new_cue));
        }

        // Get new album art
        const QUrl art_automatic = ArtForSong(file, album_art);

        if (new_cue.isEmpty() || new_cue_mtime == 0) {  // If no CUE or it's about to lose it.
          UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, matching_songs_has_cue && new_cue_mtime == 0, t);
        }
        else {  // If CUE associated.
          UpdateCueAssociatedSongs(file, path, fingerprint, new_cue, art_automatic, matching_songs, t);
        }

      }
      else {  // The song is on disk but not in the DB

        const SongList songs = ScanNewFile(file, path, fingerprint, new_cue, &cues_processed);
        if (songs.isEmpty()) {
          t->AddToProgress(1);
          continue;
        }

        qLog(Debug) << file << "is new.";

        // Choose art for the song(s)
        const QUrl art_automatic = ArtForSong(file, album_art);

        for (Song song : songs) {
          song.set_directory_id(t->dir());
          if (song.art_automatic().isEmpty()) song.set_art_automatic(art_automatic);
          t->new_songs << song;
        }
      }
    }
    t->AddToProgress(1);
  }

  // Look for deleted songs
  for (const Song &song : std::as_const(songs_in_db)) {
    QString file = song.url().toLocalFile();
    if (!song.unavailable() && !files_on_disk.contains(file) && !t->files_changed_path_.contains(file)) {
      qLog(Debug) << "Song deleted from disk:" << file;
      t->deleted_songs << song;
    }
  }

  // Add this subdir to the new or touched list
  CollectionSubdirectory updated_subdir;
  updated_subdir.directory_id = t->dir();
  updated_subdir.mtime = path_info.exists() ? path_info.lastModified().toSecsSinceEpoch() : 0;
  updated_subdir.path = path;

  if (subdir.directory_id == -1) {
    t->new_subdirs << updated_subdir;
  }
  else {
    t->touched_subdirs << updated_subdir;
  }

  if (updated_subdir.mtime == 0) {  // CollectionSubdirectory deleted, mark it for removal from the watcher.
    t->deleted_subdirs << updated_subdir;
  }

  // Recurse into the new subdirs that we found
  for (const CollectionSubdirectory &my_new_subdir : std::as_const(my_new_subdirs)) {
    if (stop_or_abort_requested()) return;
    ScanSubdirectory(my_new_subdir.path, my_new_subdir, 0, t, true);
  }

}

void CollectionWatcher::UpdateCueAssociatedSongs(const QString &file,
                                                 const QString &path,
                                                 const QString &fingerprint,
                                                 const QString &matching_cue,
                                                 const QUrl &art_automatic,
                                                 const SongList &old_cue_songs,
                                                 ScanTransaction *t) const {

  QHash<quint64, Song> sections_map;
  for (const Song &song : old_cue_songs) {
    sections_map.insert(static_cast<quint64>(song.beginning_nanosec()), song);
  }

  // Load new CUE songs
  QFile cue_file(matching_cue);
  if (!cue_file.exists()) return;
  if (!cue_file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open CUE file" << matching_cue << "for reading:" << cue_file.errorString();
    return;
  }
  const SongList songs = cue_parser_->Load(&cue_file, matching_cue, path, false).songs;
  cue_file.close();

  // Update every song that's in the CUE and collection
  QSet<int> used_ids;
  for (Song new_cue_song : songs) {
    new_cue_song.set_source(source_);
    new_cue_song.set_directory_id(t->dir());
    PerformEBUR128Analysis(new_cue_song);
    new_cue_song.set_fingerprint(fingerprint);

    if (sections_map.contains(static_cast<quint64>(new_cue_song.beginning_nanosec()))) {  // Changed section
      const Song matching_cue_song = sections_map[static_cast<quint64>(new_cue_song.beginning_nanosec())];
      new_cue_song.set_id(matching_cue_song.id());
      new_cue_song.set_art_automatic(art_automatic);
      new_cue_song.MergeUserSetData(matching_cue_song, true, true);
      AddChangedSong(file, matching_cue_song, new_cue_song, t);
      used_ids.insert(matching_cue_song.id());
    }
    else {  // A new section
      t->new_songs << new_cue_song;
    }
  }

  // Sections that are now missing
  for (const Song &old_cue : old_cue_songs) {
    if (!used_ids.contains(old_cue.id())) {
      t->deleted_songs << old_cue;
    }
  }

}

void CollectionWatcher::UpdateNonCueAssociatedSong(const QString &file,
                                                   const QString &fingerprint,
                                                   const SongList &matching_songs,
                                                   const QUrl &art_automatic,
                                                   const bool cue_deleted,
                                                   ScanTransaction *t) {

  // If a CUE got deleted, we turn it's first section into the new 'raw' (cueless) song, and we just remove the rest of the sections from the collection
  const Song &matching_song = matching_songs.first();
  if (cue_deleted) {
    for (const Song &song : matching_songs) {
      if (!song.IsAllMetadataEqual(matching_song)) {
        t->deleted_songs << song;
      }
    }
  }

  Song song_on_disk(source_);
  const TagReaderResult result = tagreader_client_->ReadFileBlocking(file, &song_on_disk);
  if (result.success() && song_on_disk.is_valid()) {
    song_on_disk.set_source(source_);
    song_on_disk.set_directory_id(t->dir());
    song_on_disk.set_id(matching_song.id());
    PerformEBUR128Analysis(song_on_disk);
    song_on_disk.set_fingerprint(fingerprint);
    song_on_disk.set_art_automatic(art_automatic);
    song_on_disk.MergeUserSetData(matching_song, !overwrite_playcount_, !overwrite_rating_);
    AddChangedSong(file, matching_song, song_on_disk, t);
  }

}

SongList CollectionWatcher::ScanNewFile(const QString &file, const QString &path, const QString &fingerprint, const QString &matching_cue, QSet<QString> *cues_processed) const {

  SongList songs;

  quint64 matching_cue_mtime = GetMtimeForCue(matching_cue);
  if (matching_cue_mtime != 0) {  // If it's a CUE - create virtual tracks

    // Don't process the same CUE many times
    if (cues_processed->contains(matching_cue)) return songs;

    QFile cue_file(matching_cue);
    if (!cue_file.exists()) return songs;

    if (!cue_file.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Could not open CUE file" << matching_cue << "for reading:" << cue_file.errorString();
      return songs;
    }

    // Ignore FILEs pointing to other media files.
    // Also, watch out for incorrect media files.
    // Playlist parser for CUEs considers every entry in sheet valid, and we don't want invalid media getting into collection!
    QString file_nfd = file.normalized(QString::NormalizationForm_D);
    SongList cue_songs = cue_parser_->Load(&cue_file, matching_cue, path, false).songs;
    cue_file.close();
    songs.reserve(cue_songs.count());
    for (Song &cue_song : cue_songs) {
      cue_song.set_source(source_);
      PerformEBUR128Analysis(cue_song);
      cue_song.set_fingerprint(fingerprint);
      if (cue_song.url().toLocalFile().normalized(QString::NormalizationForm_D) == file_nfd) {
        songs << cue_song;
      }
    }
    if (!songs.isEmpty()) {
      *cues_processed << matching_cue;
    }
  }
  else {  // It's a normal media file
    Song song(source_);
    const TagReaderResult result = tagreader_client_->ReadFileBlocking(file, &song);
    if (result.success() && song.is_valid()) {
      song.set_source(source_);
      PerformEBUR128Analysis(song);
      song.set_fingerprint(fingerprint);
      songs << song;
    }
  }

  return songs;

}

void CollectionWatcher::AddChangedSong(const QString &file, const Song &matching_song, const Song &new_song, ScanTransaction *t) {

  bool notify_new = false;
  QStringList changes;

  if (matching_song.unavailable()) {
    qLog(Debug) << "Unavailable song" << file << "restored.";
    notify_new = true;
  }
  else {
    if (matching_song.url() != new_song.url()) {
      changes << u"file path"_s;
      notify_new = true;
    }
    if (matching_song.filetype() != new_song.filetype()) {
      changes << u"filetype"_s;
      notify_new = true;
    }
    if (matching_song.filesize() != new_song.filesize()) {
      changes << u"filesize"_s;
      notify_new = true;
    }
    if (matching_song.length_nanosec() != new_song.length_nanosec()) {
      changes << u"length"_s;
      notify_new = true;
    }
    if (matching_song.fingerprint() != new_song.fingerprint()) {
      changes << u"fingerprint"_s;
      notify_new = true;
    }
    if (!matching_song.IsMetadataEqual(new_song)) {
      changes << u"metadata"_s;
      notify_new = true;
    }
    if (!matching_song.IsPlayStatisticsEqual(new_song)) {
      changes << u"play statistics"_s;
      notify_new = true;
    }
    if (!matching_song.IsRatingEqual(new_song)) {
      changes << u"rating"_s;
      notify_new = true;
    }
    if (!matching_song.IsArtEqual(new_song)) {
      changes << u"album art"_s;
      notify_new = true;
    }
    if (!matching_song.IsAcoustIdEqual(new_song)) {
      changes << u"acoustid"_s;
      notify_new = true;
    }
    if (!matching_song.IsMusicBrainzEqual(new_song)) {
      changes << u"musicbrainz"_s;
      notify_new = true;
    }
    if (!matching_song.IsEBUR128Equal(new_song)) {
      changes << u"ebur128 loudness characteristics"_s;
      notify_new = true;
    }
    if (matching_song.mtime() != new_song.mtime()) {
      changes << u"mtime"_s;
    }
    if (matching_song.ctime() != new_song.ctime()) {
      changes << u"ctime"_s;
    }

    if (changes.isEmpty()) {
      qLog(Debug) << "Song" << file << "unchanged.";
    }
    else {
      qLog(Debug) << "Song" << file << changes.join(", "_L1) << "changed.";
    }

  }

  if (notify_new) {
    t->new_songs << new_song;
  }
  else {
    t->touched_songs << new_song;
  }

}

void CollectionWatcher::PerformEBUR128Analysis(Song &song) const {

  if (!song_ebur128_loudness_analysis_) return;

#ifdef HAVE_EBUR128
  std::optional<EBUR128Measures> loudness_characteristics = EBUR128Analysis::Compute(song);
  if (loudness_characteristics) {
    song.set_ebur128_integrated_loudness_lufs(loudness_characteristics->loudness_lufs);
    song.set_ebur128_loudness_range_lu(loudness_characteristics->range_lu);
  }
#else
  Q_UNUSED(song)
#endif

}

quint64 CollectionWatcher::GetMtimeForCue(const QString &cue_path) {

  if (cue_path.isEmpty()) {
    return 0;
  }

  const QFileInfo fileinfo(cue_path);
  if (!fileinfo.exists()) {
    return 0;
  }

  const QDateTime cue_last_modified = fileinfo.lastModified();

  return cue_last_modified.isValid() ? static_cast<quint64>(cue_last_modified.toSecsSinceEpoch()) : 0;

}

void CollectionWatcher::AddWatch(const CollectionDirectory &dir, const QString &path) {

  if (!QFile::exists(path)) return;

  fs_watcher_->AddPath(path);
  subdir_mapping_[path] = dir;

}

void CollectionWatcher::RemoveWatch(const CollectionDirectory &dir, const CollectionSubdirectory &subdir) {

  const QStringList subdir_paths = subdir_mapping_.keys(dir);
  for (const QString &subdir_path : subdir_paths) {
    if (subdir_path != subdir.path) continue;
    fs_watcher_->RemovePath(subdir_path);
    subdir_mapping_.remove(subdir_path);
    break;
  }

}

void CollectionWatcher::RemoveDirectory(const CollectionDirectory &dir) {

  rescan_queue_.remove(dir.id);
  watched_dirs_.remove(dir.id);

  // Stop watching the directory's subdirectories
  const QStringList subdir_paths = subdir_mapping_.keys(dir);
  for (const QString &subdir_path : subdir_paths) {
    fs_watcher_->RemovePath(subdir_path);
    subdir_mapping_.remove(subdir_path);
  }

}

bool CollectionWatcher::FindSongsByPath(const SongList &songs, const QString &path, SongList *out) {

  for (const Song &song : songs) {
    if (song.url().toLocalFile() == path) {
      *out << song;
    }
  }

  return !out->isEmpty();

}

bool CollectionWatcher::FindSongsByFingerprint(const QString &file, const QString &fingerprint, SongList *out) {

  const SongList songs = backend_->GetSongsByFingerprint(fingerprint);
  for (const Song &song : songs) {
    QString filename = song.url().toLocalFile();
    QFileInfo info(filename);
    // Allow multiple songs in different directories with the same fingerprint.
    // Only use the matching song by fingerprint if it doesn't already exist in a different path.
    if (file == filename || !info.exists()) {
      *out << song;
    }
  }

  return !out->isEmpty();

}

bool CollectionWatcher::FindSongsByFingerprint(const QString &file, const SongList &songs, const QString &fingerprint, SongList *out) {

  for (const Song &song : songs) {
    QString filename = song.url().toLocalFile();
    if (song.fingerprint() == fingerprint && (file == filename || !QFileInfo::exists(filename))) {
      *out << song;
      return true;
    }
  }

  return !out->isEmpty();

}

void CollectionWatcher::DirectoryChanged(const QString &subdir) {

  // Find what dir it was in
  QHash<QString, CollectionDirectory>::const_iterator it = subdir_mapping_.constFind(subdir);
  if (it == subdir_mapping_.constEnd()) {
    return;
  }
  const CollectionDirectory dir = *it;

  qLog(Debug) << "Subdir" << subdir << "changed under directory" << dir.path << "id" << dir.id;

  // Queue the subdir for rescanning
  if (!rescan_queue_[dir.id].contains(subdir)) rescan_queue_[dir.id] << subdir;

  if (!rescan_paused_) rescan_timer_->start();

}

void CollectionWatcher::RescanPathsNow() {

  const QList<int> dirs = rescan_queue_.keys();
  for (const int dir : dirs) {
    if (stop_or_abort_requested()) break;
    ScanTransaction transaction(this, dir, false, false, mark_songs_unavailable_);

    const QStringList paths = rescan_queue_.value(dir);

    QMap<QString, quint64> subdir_files_count;
    for (const QString &path : paths) {
      const quint64 files_count = FilesCountForPath(&transaction, path);
      subdir_files_count[path] = files_count;
      transaction.AddToProgressMax(files_count);
    }

    for (const QString &path : paths) {
      if (stop_or_abort_requested()) break;
      CollectionSubdirectory subdir;
      subdir.directory_id = dir;
      subdir.mtime = 0;
      subdir.path = path;
      ScanSubdirectory(path, subdir, subdir_files_count[path], &transaction);
    }
  }

  rescan_queue_.clear();

  Q_EMIT CompilationsNeedUpdating();

}

QString CollectionWatcher::PickBestArt(const QStringList &art_automatic_list) {

  // This is used when there is more than one image in a directory.
  // Pick the biggest image that matches the most important filter

  QStringList filtered;

  for (const QString &filter_text : std::as_const(best_art_filters_)) {
    // The images in the images list are represented by a full path, so we need to isolate just the filename
    for (const QString &art_automatic : art_automatic_list) {
      QFileInfo fileinfo(art_automatic);
      QString filename(fileinfo.fileName());
      if (filename.contains(filter_text, Qt::CaseInsensitive))
        filtered << art_automatic;
    }

    // We assume the filters are give in the order best to worst, so if we've got a result, we go with it.
    // Otherwise we might start capturing more generic rules
    if (!filtered.isEmpty()) break;
  }

  if (filtered.isEmpty()) {
    // The filter was too restrictive, just use the original list
    filtered = art_automatic_list;
  }

  int biggest_size = 0;
  QString biggest_path;

  for (const QString &path : std::as_const(filtered)) {
    if (stop_or_abort_requested()) break;

    QImage image(path);
    if (image.isNull()) continue;

    int size = image.width() * image.height();
    if (size > biggest_size) {
      biggest_size = size;
      biggest_path = path;
    }
  }

  return biggest_path;

}

QUrl CollectionWatcher::ArtForSong(const QString &path, QMap<QString, QStringList> &art_automatic_list) {

  QString dir(DirectoryPart(path));

  if (art_automatic_list.contains(dir)) {
    if (art_automatic_list[dir].count() == 1) {
      return QUrl::fromLocalFile(art_automatic_list[dir][0]);
    }
    else {
      const QString best_art = PickBestArt(art_automatic_list[dir]);
      art_automatic_list[dir] = QStringList() << best_art;
      return QUrl::fromLocalFile(best_art);
    }
  }

  return QUrl();

}

void CollectionWatcher::SetRescanPausedAsync(bool pause) {

  QMetaObject::invokeMethod(this, "SetRescanPaused", Qt::QueuedConnection, Q_ARG(bool, pause));

}

void CollectionWatcher::SetRescanPaused(bool pause) {

  rescan_paused_ = pause;
  if (!rescan_paused_ && !rescan_queue_.isEmpty()) RescanPathsNow();

}

void CollectionWatcher::IncrementalScanAsync() {

  QMetaObject::invokeMethod(this, &CollectionWatcher::IncrementalScanNow, Qt::QueuedConnection);

}

void CollectionWatcher::FullScanAsync() {

  QMetaObject::invokeMethod(this, &CollectionWatcher::FullScanNow, Qt::QueuedConnection);

}

void CollectionWatcher::IncrementalScanCheck() {

  qint64 duration = QDateTime::currentSecsSinceEpoch() - last_scan_time_;
  if (duration >= 86400) {
    qLog(Debug) << "Performing periodic incremental scan.";
    IncrementalScanNow();
  }

}

void CollectionWatcher::IncrementalScanNow() { PerformScan(true, false); }

void CollectionWatcher::FullScanNow() { PerformScan(false, true); }

void CollectionWatcher::PerformScan(const bool incremental, const bool ignore_mtimes) {

  CancelStop();

  for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {

    if (stop_or_abort_requested()) break;

    ScanTransaction transaction(this, dir.id, incremental, ignore_mtimes, mark_songs_unavailable_);
    CollectionSubdirectoryList subdirs = transaction.GetAllSubdirs();

    if (subdirs.isEmpty()) {
      qLog(Debug) << "Collection directory wasn't in subdir list.";
      CollectionSubdirectory subdir;
      subdir.path = dir.path;
      subdir.directory_id = dir.id;
      subdirs << subdir;
    }

    QMap<QString, quint64> subdir_files_count;
    quint64 files_count = FilesCountForSubdirs(&transaction, subdirs, subdir_files_count);
    transaction.AddToProgressMax(files_count);

    for (const CollectionSubdirectory &subdir : std::as_const(subdirs)) {
      if (stop_or_abort_requested()) break;
      ScanSubdirectory(subdir.path, subdir, subdir_files_count[subdir.path], &transaction);
    }

  }

  last_scan_time_ = QDateTime::currentSecsSinceEpoch();

  Q_EMIT CompilationsNeedUpdating();

}

quint64 CollectionWatcher::FilesCountForPath(ScanTransaction *t, const QString &path) {

  const QFileInfo path_info(path);
  if (path_info.isSymLink()) {
    const QString real_path = path_info.symLinkTarget();
    const QStorageInfo storage_info(real_path);
    if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
      return 0;
    }
    for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
      if (real_path.startsWith(dir.path)) {
        return 0;
      }
    }
  }
  else {
    const QStorageInfo storage_info(path);
    if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
      return 0;
    }
  }

  quint64 i = 0;
  QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  while (it.hasNext()) {

    if (stop_or_abort_requested()) break;

    const QString child_filepath = it.next();
    const QFileInfo child_fileinfo(child_filepath);

    if (child_fileinfo.isDir()) {
      if (child_fileinfo.isSymLink()) {

        const QString real_path = child_fileinfo.symLinkTarget();

        QStorageInfo storage_info(real_path);
        if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
          continue;
        }

        for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
          if (real_path.startsWith(dir.path)) {
            continue;
          }
        }
      }

      if (!t->HasSeenSubdir(child_filepath) && !child_fileinfo.isHidden()) {
        // We haven't seen this subdirectory before, so we need to include the file count for this directory too.
        i += FilesCountForPath(t, child_filepath);
      }

    }

    ++i;

  }

  return i;

}

quint64 CollectionWatcher::FilesCountForSubdirs(ScanTransaction *t, const CollectionSubdirectoryList &subdirs, QMap<QString, quint64> &subdir_files_count) {

  quint64 i = 0;
  for (const CollectionSubdirectory &subdir : subdirs) {
    if (stop_or_abort_requested()) break;
    const quint64 files_count = FilesCountForPath(t, subdir.path);
    subdir_files_count[subdir.path] = files_count;
    i += files_count;
  }

  return i;

}

void CollectionWatcher::RescanSongsAsync(const SongList &songs) {

  QMetaObject::invokeMethod(this, "RescanSongs", Qt::QueuedConnection, Q_ARG(SongList, songs));

}

void CollectionWatcher::RescanSongs(const SongList &songs) {

  CancelStop();

  QStringList scanned_paths;
  for (const Song &song : songs) {
    if (stop_or_abort_requested()) break;
    const QString song_path = song.url().toLocalFile().section(u'/', 0, -2);
    if (scanned_paths.contains(song_path)) continue;
    ScanTransaction transaction(this, song.directory_id(), false, true, mark_songs_unavailable_);
    const CollectionSubdirectoryList subdirs = transaction.GetAllSubdirs();
    for (const CollectionSubdirectory &subdir : subdirs) {
      if (stop_or_abort_requested()) break;
      if (subdir.path != song_path) continue;
      qLog(Debug) << "Rescan for directory ID" << song.directory_id() << "directory" << subdir.path;
      const quint64 files_count = FilesCountForPath(&transaction, subdir.path);
      ScanSubdirectory(song_path, subdir, files_count, &transaction);
      scanned_paths << subdir.path;
    }
  }

  Q_EMIT CompilationsNeedUpdating();

}
