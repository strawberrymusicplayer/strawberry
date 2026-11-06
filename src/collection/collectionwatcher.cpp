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

#include <algorithm>
#include <utility>
#include <chrono>
#include <optional>

#include <QObject>
#include <QThread>
#include <QIODevice>
#include <QStorageInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
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

#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/settings.h"
#include "utilities/imageutils.h"
#include "utilities/textencodingutils.h"
#include "utilities/fileutils.h"
#include "utilities/lrcutils.h"
#include "constants/timeconstants.h"
#include "constants/filesystemconstants.h"
#include "tagreader/tagreaderclient.h"
#include "collectiondirectory.h"
#include "collectionbackend.h"
#include "collectionwatcher.h"
#include "playlistparsers/cueparser.h"
#include "constants/collectionsettings.h"
#include "engine/ebur128measures.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  include "core/filesystemwatcherinotify.h"
#elif defined(Q_OS_WIN32)
#  include "core/filesystemwatcherwin.h"
#else
#  include "core/filesystemwatcherqt.h"
#endif

#ifdef HAVE_SONGTRACKING
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

namespace {

// LRC files are small text files, anything larger is not read into memory.
constexpr qint64 kMaxLrcFileSize = 1024 * 1024;

const QStringList &ValidImageExtensions() {

  static const QStringList valid_image_extensions = []() {
    QStringList result = QStringList() << u"jpg"_s << u"jpeg"_s << u"jp2"_s << u"png"_s << u"gif"_s << u"tiff"_s << u"tif"_s << u"webp"_s;
    const QStringList &supported_image_formats = ImageUtils::SupportedImageFormats();
    for (const QString &supported_image_format : supported_image_formats) {
      if (!result.contains(supported_image_format)) {
        result.append(supported_image_format);
      }
    }
    return result;
  }();

  return valid_image_extensions;

}

}  // namespace

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
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
      fs_watcher_(new FileSystemWatcherInotify(this)),
#elif defined(Q_OS_WIN32)
      fs_watcher_(new FileSystemWatcherWin(this)),
#else
      fs_watcher_(new FileSystemWatcherQt(this)),
#endif
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
      overflow_scan_pending_(false),
      total_watches_(0),
      cue_parser_(new CueParser(tagreader_client, backend, this)),
      last_scan_time_(0) {

  setObjectName(source_ == Song::Source::Collection ? QLatin1String(QObject::metaObject()->className()) : QStringLiteral("%1%2").arg(Song::DescriptionForSource(source_), QLatin1String(QObject::metaObject()->className())));

  original_thread_ = thread();

  rescan_timer_->setInterval(2s);
  rescan_timer_->setSingleShot(true);

  periodic_scan_timer_->setInterval(86400 * kMsecPerSec);
  periodic_scan_timer_->setSingleShot(false);

  ReloadSettings();

  QObject::connect(fs_watcher_, &FileSystemWatcherInterface::PathChanged, this, &CollectionWatcher::DirectoryChanged, Qt::UniqueConnection);
  QObject::connect(fs_watcher_, &FileSystemWatcherInterface::Overflow, this, &CollectionWatcher::FileSystemWatcherOverflow, Qt::UniqueConnection);
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
    scan_on_startup_ = s.value(CollectionSettings::kStartupScan, CollectionSettings::kDefaultStartupScan).toBool();
    monitor_ = s.value(CollectionSettings::kMonitor, CollectionSettings::kDefaultMonitor).toBool();
  }
  else {
    scan_on_startup_ = true;
    monitor_ = true;
  }
  const QStringList filters = s.value(CollectionSettings::kCoverArtPatterns, QStringList() << u"front"_s << u"cover"_s).toStringList();
  if (source_ == Song::Source::Collection) {
    song_tracking_ = s.value(CollectionSettings::kSongTracking, CollectionSettings::kDefaultSongTracking).toBool();
    song_ebur128_loudness_analysis_ = s.value(CollectionSettings::kSongENUR128LoudnessAnalysis, CollectionSettings::kDefaultSongENUR128LoudnessAnalysis).toBool();
    mark_songs_unavailable_ = song_tracking_ ? true : s.value(CollectionSettings::kMarkSongsUnavailable, CollectionSettings::kDefaultMarkSongsUnavailable).toBool();
  }
  else {
    song_tracking_ = false;
    song_ebur128_loudness_analysis_ = false;
    mark_songs_unavailable_ = false;
  }
  expire_unavailable_songs_days_ = s.value(CollectionSettings::kExpireUnavailableSongs, CollectionSettings::kDefaultExpireUnavailableSongs).toInt();
  overwrite_playcount_ = s.value(CollectionSettings::kOverwritePlaycount, CollectionSettings::kDefaultOverwritePlaycount).toBool();
  overwrite_rating_ = s.value(CollectionSettings::kOverwriteRating, CollectionSettings::kDefaultOverwriteRating).toBool();
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

CollectionWatcher::ScanTransaction::ScanTransaction(CollectionWatcher *watcher, const int dir_id, const bool incremental, const bool ignores_mtime)
    : watcher_(watcher),
      dir_id_(dir_id),
      incremental_(incremental),
      ignores_mtime_(ignores_mtime),
      mark_songs_unavailable_(watcher->mark_songs_unavailable_),
      expire_unavailable_songs_days_(watcher->expire_unavailable_songs_days_),
      progress_max_(0),
      progress_(0),
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

void CollectionWatcher::ScanTransaction::LogChanges() const {

  // CUE songs share the media file, so the track is logged too.
  const auto song_description = [](const Song &song) {
    return song.has_cue() ? song.url().toLocalFile() + u" track "_s + QString::number(song.track()) : song.url().toLocalFile();
  };

  for (const Song &song : deleted_songs) {
    if (mark_songs_unavailable_ && watcher_->source() == Song::Source::Collection) {
      qLog(Debug) << "Song unavailable:" << song_description(song);
    }
    else {
      qLog(Debug) << "Song deleted:" << song_description(song);
    }
  }
  for (const Song &song : new_songs) {
    if (song.id() == -1) {
      qLog(Debug) << "Song added:" << song_description(song);
    }
    else {
      qLog(Debug) << "Song changed:" << song_description(song) << song_changes.value(song.id()).join(", "_L1);
    }
  }
  for (const Song &song : touched_songs) {
    qLog(Debug) << "Song unchanged:" << song_description(song);
  }
  for (const Song &song : readded_songs) {
    qLog(Debug) << "Song restored without rescanning:" << song_description(song);
  }

}

void CollectionWatcher::ScanTransaction::CommitNewOrUpdatedSongs() {

  if (!deleted_subdirs.isEmpty()) {
    Q_EMIT watcher_->SubdirsDeleted(deleted_subdirs);
  }

  if (!new_subdirs.isEmpty()) {
    Q_EMIT watcher_->SubdirsDiscovered(new_subdirs);
  }

  if (!touched_subdirs.isEmpty()) {
    Q_EMIT watcher_->SubdirsMTimeUpdated(touched_subdirs);
    touched_subdirs.clear();
  }

  // Songs which are updated or restored in this transaction are not deleted, for example songs found by fingerprint which changed file path, which are also deleted when their old file path is scanned in this transaction.
  // Each song is only deleted once.
  if (!deleted_songs.isEmpty()) {
    QSet<int> kept_song_ids;
    for (const SongList *songs : {&new_songs, &touched_songs, &readded_songs}) {
      for (const Song &song : *songs) {
        if (song.id() != -1) kept_song_ids.insert(song.id());
      }
    }
    SongList songs_to_delete;
    QSet<int> deleted_song_ids;
    for (const Song &song : std::as_const(deleted_songs)) {
      if (kept_song_ids.contains(song.id()) || deleted_song_ids.contains(song.id())) continue;
      deleted_song_ids.insert(song.id());
      songs_to_delete << song;
    }
    deleted_songs = songs_to_delete;
  }

  LogChanges();

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
    song_changes.clear();
  }

  if (!touched_songs.isEmpty()) {
    Q_EMIT watcher_->SongsMTimeUpdated(touched_songs);
    touched_songs.clear();
  }

  if (!readded_songs.isEmpty()) {
    Q_EMIT watcher_->SongsReadded(readded_songs);
    readded_songs.clear();
  }

  for (const CollectionSubdirectory &subdir : std::as_const(deleted_subdirs)) {
    if (watcher_->watched_dirs_.contains(dir_id_)) {
      watcher_->RemoveWatch(watcher_->watched_dirs_[dir_id_], subdir);
    }
  }
  deleted_subdirs.clear();

  if (watcher_->monitor_) {
    for (const CollectionSubdirectory &subdir : std::as_const(new_subdirs)) {
      if (watcher_->watched_dirs_.contains(dir_id_)) {
        watcher_->AddWatch(watcher_->watched_dirs_[dir_id_], subdir.path);
      }
    }
  }
  new_subdirs.clear();

  if (incremental_ || ignores_mtime_) {
    Q_EMIT watcher_->UpdateLastSeen(dir_id_, expire_unavailable_songs_days_);
  }

}


SongList CollectionWatcher::ScanTransaction::FindSongsInSubdirectory(const QString &path) {

  if (cached_songs_dirty_) {
    const SongList songs = watcher_->backend_->FindSongsInDirectory(dir_id_);
    for (const Song &song : songs) {
      // Key by the NFC-normalized parent directory so a file that differs only in Unicode normalization (NFC vs NFD) between disk and database still matches
      const QString p = song.url().toLocalFile().normalized(QString::NormalizationForm_C).section(u'/', 0, -2);
      cached_songs_.insert(p, song);
    }
    cached_songs_dirty_ = false;
  }

  const QString nfc_path = path.normalized(QString::NormalizationForm_C);
  if (cached_songs_.contains(nfc_path)) {
    return cached_songs_.values(nfc_path);
  }

  return SongList();

}

bool CollectionWatcher::ScanTransaction::HasSongsWithMissingFingerprint(const QString &path) {

  if (cached_songs_missing_fingerprint_dirty_) {
    const SongList songs = watcher_->backend_->SongsWithMissingFingerprint(dir_id_);
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
    const SongList songs = watcher_->backend_->SongsWithMissingLoudnessCharacteristics(dir_id_);
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

  known_subdir_paths_.clear();
  known_subdirs_by_parent_.clear();
  for (const CollectionSubdirectory &subdir : subdirs) {
    // Subdirectories without a modification time are not seen, so they are scanned, but they are still children, so they are found when they're removed.
    if (subdir.mtime != 0) {
      known_subdir_paths_.insert(subdir.path);
    }
    // QFileInfo::path() keeps the slash of a root directory, e.g. "/" for "/Music" and "C:/" for "C:/Music".
    known_subdirs_by_parent_[QFileInfo(subdir.path).path()] << subdir;
  }

}

bool CollectionWatcher::ScanTransaction::HasSeenSubdir(const QString &path) {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_id_));
  }

  return known_subdir_paths_.contains(path);

}

bool CollectionWatcher::ScanTransaction::HasScannedPath(const QString &path) {

  return scanned_paths_.contains(path);

}

void CollectionWatcher::ScanTransaction::MarkPathScanned(const QString &path) {

  scanned_paths_.insert(path);

}

CollectionSubdirectoryList CollectionWatcher::ScanTransaction::GetImmediateSubdirs(const QString &path) {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_id_));
  }

  return known_subdirs_by_parent_.value(path);

}

CollectionSubdirectoryList CollectionWatcher::ScanTransaction::GetAllSubdirs() {

  if (known_subdirs_dirty_) {
    SetKnownSubdirs(watcher_->backend_->SubdirsInDirectory(dir_id_));
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
    ScanTransaction transaction(this, dir.id, false, false);
    const quint64 files_count = FilesCountForPath(&transaction, dir.path);
    transaction.SetKnownSubdirs(subdirs);
    transaction.AddToProgressMax(files_count);
    ScanSubdirectory(dir, dir.path, CollectionSubdirectory(), files_count, &transaction);
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
      ScanTransaction transaction(this, dir.id, true, false);
      QMap<QString, quint64> subdir_files_count;
      const quint64 files_count = FilesCountForSubdirs(&transaction, subdirs, subdir_files_count);
      transaction.SetKnownSubdirs(subdirs);
      transaction.AddToProgressMax(files_count);
      for (const CollectionSubdirectory &subdir : subdirs) {
        if (stop_or_abort_requested()) break;
        ScanSubdirectory(dir, subdir.path, subdir, subdir_files_count[subdir.path], &transaction);
      }
      if (!stop_or_abort_requested()) {
        last_scan_time_ = QDateTime::currentSecsSinceEpoch();
      }
    }
  }

  Q_EMIT CompilationsNeedUpdating();

}

void CollectionWatcher::ScanSubdirectory(const CollectionDirectory &dir, const QString &path, const CollectionSubdirectory &subdir, const quint64 files_count, ScanTransaction *t, const bool force_noincremental) {

  // A renamed directory can be reached both from its own queued change notification and from its parent's vanished-child check below, so skip it if it was already scanned in this transaction to avoid deleting/adding its songs twice.
  if (t->HasScannedPath(path)) {
    t->AddToProgress(files_count);
    return;
  }
  t->MarkPathScanned(path);

  const QFileInfo path_info(path);
  const qint64 path_mtime = path_info.exists() && path_info.lastModified().isValid() ? path_info.lastModified().toSecsSinceEpoch() : 0;

  if (path_info.isSymLink()) {
    const QString real_path = path_info.symLinkTarget();
    const QStorageInfo storage_info(real_path);
    if (kRejectedFileSystems.contains(storage_info.fileSystemType())) {
      qLog(Warning) << "Ignoring symbolic link" << path << "which links to" << real_path << "with rejected filesystem type" << storage_info.fileSystemType();
      return;
    }
    // Do not scan symlinked dirs that are already in collection
    for (const CollectionDirectory &i : std::as_const(watched_dirs_)) {
      if (real_path == i.path || real_path.startsWith(i.path + u'/')) {
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
#ifdef HAVE_SONGTRACKING
  if (song_tracking_) {
    songs_missing_fingerprint = t->HasSongsWithMissingFingerprint(path);
  }
#endif
#ifdef HAVE_EBUR128
  if (song_ebur128_loudness_analysis_) {
    songs_missing_loudness_characteristics = t->HasSongsWithMissingLoudnessCharacteristics(path);
  }
#endif

  if (!t->ignores_mtime() && !force_noincremental && t->is_incremental() && path_mtime != 0 && subdir.mtime == path_mtime && !songs_missing_fingerprint && !songs_missing_loudness_characteristics) {
    // The directory hasn't changed since last time
    t->AddToProgress(files_count);
    return;
  }

  QMap<QString, QStringList> album_art;
  QMap<QString, QString> lrc_files;
  QStringList cue_files;
  // The media files found in the directory, not changed after listing the directory.
  QStringList initial_files_on_disk;
  CollectionSubdirectoryList my_new_subdirs;

  // If a directory is moved then only its parent gets a changed notification, so we need to look and see if any of our children don't exist anymore.
  // If one has been removed, "rescan" it to get the deleted songs
  const CollectionSubdirectoryList previous_subdirs = t->GetImmediateSubdirs(path);
  for (const CollectionSubdirectory &prev_subdir : previous_subdirs) {
    if (!QFile::exists(prev_subdir.path) && prev_subdir.path != path) {
      ScanSubdirectory(dir, prev_subdir.path, prev_subdir, 0, t, true);
    }
  }

  // First we "quickly" get a list of the files in the directory that we think might be music.  While we're here, we also look for new subdirectories and possible album artwork.
  if (path_info.exists()) {
    QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    while (it.hasNext()) {

      if (stop_or_abort_requested()) return;

      const QString child_filepath = it.next();
      const QFileInfo child_fileinfo(child_filepath);

      if (child_fileinfo.isSymLink()) {
        const QStorageInfo storage_info(child_fileinfo.symLinkTarget());
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
          new_subdir.mtime = child_fileinfo.exists() && child_fileinfo.lastModified().isValid() ? child_fileinfo.lastModified().toSecsSinceEpoch() : 0;
          my_new_subdirs << new_subdir;
        }
        t->AddToProgress(1);
      }
      else {
        const QString ext_part = ExtensionPart(child_filepath);
        const QString dir_part = DirectoryPart(child_filepath);
        if (ext_part == "lrc"_L1) {
          // Synchronized lyrics for the media file with the same name, they are read together with the song.
          lrc_files.insert(NoExtensionPart(child_filepath), child_filepath);
          t->AddToProgress(1);
        }
        else if (ext_part == "cue"_L1) {
          // CUE sheets are loaded before the media files, since a CUE sheet can have songs for several media files.
          cue_files << child_filepath;
          t->AddToProgress(1);
        }
        else if (Song::kRejectedExtensions.contains(child_fileinfo.suffix(), Qt::CaseInsensitive) || child_fileinfo.baseName() == "qt_temp"_L1) {
          t->AddToProgress(1);
        }
        else if (ValidImageExtensions().contains(ext_part)) {
          album_art[dir_part] << child_filepath;
          t->AddToProgress(1);
        }
        else {
          initial_files_on_disk << child_filepath;
        }
      }
    }
  }

  if (stop_or_abort_requested()) return;

  // Ask the database for a list of files in this directory
  // FindSongsInSubdirectory() finds the songs by the directory in NFC, the songs of another directory on disk whose name only differs in Unicode normalization form are not included.
  // The songs of another spelling of this directory are, also with the difference in a parent directory, the directories are compared with every component resolved to the names on disk.
  SongList songs_in_db;
  {
    QHash<QString, bool> same_directory;
    // Only resolved when a song has another spelling of the directory, which is rare.
    QString path_on_disk;
    const SongList songs_in_directory = t->FindSongsInSubdirectory(path);
    for (const Song &song : songs_in_directory) {
      const QString song_directory = song.url().toLocalFile().section(u'/', 0, -2);
      if (song_directory == path) {
        songs_in_db << song;
        continue;
      }
      QHash<QString, bool>::const_iterator it = same_directory.constFind(song_directory);
      if (it == same_directory.constEnd()) {
        // A directory which isn't on disk with this spelling is not another directory.
        bool same = !QFileInfo::exists(song_directory);
        if (!same) {
          if (path_on_disk.isEmpty()) path_on_disk = OnDiskFilename(QDir::cleanPath(path), &t->directory_entries_);
          same = OnDiskFilename(QDir::cleanPath(song_directory), &t->directory_entries_) == path_on_disk;
        }
        it = same_directory.insert(song_directory, same);
      }
      if (it.value()) {
        songs_in_db << song;
      }
    }
  }
  const QHash<QString, SongList> songs_in_db_by_path = SongsByPath(songs_in_db);

  // Find the CUE sheets first, to know which media files have CUE sheets.
  QSet<QString> unreadable_cue_sheets;
  const QHash<QString, QString> cue_sheets = FindCueSheets(cue_files, path, &unreadable_cue_sheets, &t->directory_entries_);
  QHash<QString, QMap<QString, SongList>> loaded_cue_sheets;

  // Now compare the list from the database with the list of files on disk
  const QSet<QString> files_on_disk_set(initial_files_on_disk.constBegin(), initial_files_on_disk.constEnd());
  // The media files which are kept, the media files which are removed or couldn't be read are not added.
  QSet<QString> valid_files_on_disk;
  valid_files_on_disk.reserve(initial_files_on_disk.count());
  // The media file on disk which the songs in the database were matched with by path, for finding deleted songs.
  QHash<int, QString> matched_song_files;
  for (const QString &file : std::as_const(initial_files_on_disk)) {

    if (stop_or_abort_requested()) return;

    // Set to false when the media file is not kept, but scanning it continues.
    bool file_valid = true;

    // Associated CUE, the CUE sheet which has songs for this media file.
    const QString new_cue = cue_sheets.value(PathKey(file));

    SongList matching_songs;
    if (FindSongsByPath(songs_in_db_by_path, file, files_on_disk_set, matching_songs)) {  // Found matching song in DB by path.

      for (const Song &song : std::as_const(matching_songs)) {
        matched_song_files.insert(song.id(), file);
      }

      // The songs of a media file with a CUE sheet can be both the CUE songs and a song for the whole media file, which is used while the CUE sheet can't be read.
      // With unavailable songs kept, the CUE songs can also be from several CUE sheets, for example when another CUE sheet was used while the CUE sheet couldn't be read.
      SongList cue_songs_in_db;
      SongList file_songs_in_db;
      SongList available_cue_songs;
      SongList available_file_songs;
      SongList new_cue_songs_in_db;
      for (const Song &song : std::as_const(matching_songs)) {
        if (song.has_cue()) {
          cue_songs_in_db << song;
          if (!song.unavailable()) available_cue_songs << song;
          if (!new_cue.isEmpty() && PathKey(song.cue_path()) == PathKey(new_cue)) new_cue_songs_in_db << song;
        }
        else {
          file_songs_in_db << song;
          if (!song.unavailable()) available_file_songs << song;
        }
      }
      const bool file_song_available = !available_file_songs.isEmpty();
      const bool cue_song_available = !available_cue_songs.isEmpty();

      // The songs in use first, the song for the whole media file while the CUE sheet can't be read, otherwise the available CUE songs.
      // When no song is available, the CUE songs of the CUE sheet for the media file now, otherwise the song for the whole media file, so it's kept when the CUE sheet is deleted.
      SongList songs_in_use;
      if (file_song_available) {
        songs_in_use = available_file_songs;
      }
      else if (cue_song_available) {
        songs_in_use = available_cue_songs;
      }
      else if (!new_cue_songs_in_db.isEmpty()) {
        songs_in_use = new_cue_songs_in_db;
      }
      else if (!file_songs_in_db.isEmpty()) {
        songs_in_use = file_songs_in_db;
      }
      else {
        songs_in_use = cue_songs_in_db;
      }
      // Song::operator==() doesn't compare the CUE sheet, so compare the IDs, the song for the whole media file has the same URL and beginning as the first CUE song.
      QSet<int> songs_in_use_ids;
      for (const Song &song : std::as_const(songs_in_use)) {
        songs_in_use_ids.insert(song.id());
      }
      SongList other_songs;
      for (const Song &song : std::as_const(matching_songs)) {
        if (!songs_in_use_ids.contains(song.id())) other_songs << song;
      }
      matching_songs = songs_in_use + other_songs;

      const Song matching_song = matching_songs.constFirst();

      // The song is in the database and still on disk.
      // Check the mtime to see if it's been changed since it was added.
      const QFileInfo fileinfo(file);

      if (!fileinfo.exists()) {
        // Partially fixes race condition - if file was removed between being added to the list and now.
        t->AddToProgress(1);
        continue;
      }

      // CUE sheet's path from collection (if any), the CUE sheet of the songs in use, otherwise of the last CUE songs.
      const QString old_cue = matching_song.has_cue() ? matching_song.cue_path() : (cue_songs_in_db.isEmpty() ? QString() : cue_songs_in_db.first().cue_path());

      // The CUE sheet of the songs exists, but can't be read, mark the CUE songs unavailable, and use a song for the whole media file until it can be read again.
      // When another CUE sheet which can be read has songs for the media file, that's used instead.
      if (!old_cue.isEmpty() && unreadable_cue_sheets.contains(PathKey(old_cue)) && (new_cue.isEmpty() || PathKey(new_cue) == PathKey(old_cue))) {
        UseMediaFileWhileCueSheetUnreadable(file, fileinfo, cue_songs_in_db, file_songs_in_db, ArtForSong(file, album_art), lrc_files.value(NoExtensionPart(file)), t);
        valid_files_on_disk.insert(file);
        t->AddToProgress(1);
        continue;
      }

      // Only when the song in use has a CUE sheet, the mtime of the song for the whole media file is the mtime of the media file.
      const qint64 matching_song_cue_mtime = static_cast<qint64>(GetMtimeForPath(matching_song.cue_path()));

      // CUE sheet's path from this file (if any).
      qint64 new_cue_mtime = 0;
      if (!new_cue.isEmpty()) {
        new_cue_mtime = static_cast<qint64>(GetMtimeForPath(new_cue));
      }

      const bool cue_added = new_cue_mtime != 0 && cue_songs_in_db.isEmpty();
      // The actual paths are compared, so a CUE sheet renamed to another Unicode normalization form gets its new path stored.
      const bool cue_changed = new_cue_mtime != 0 && !cue_songs_in_db.isEmpty() && new_cue != old_cue;
      // CUE songs which are already unavailable, after the CUE sheet was deleted, are not deleted again.
      // When no song is available, for example when the media file is restored without the CUE sheet, the CUE songs are replaced with the song for the whole media file instead of restoring them.
      const bool cue_deleted = !cue_songs_in_db.isEmpty() && (cue_song_available || !file_song_available) && new_cue_mtime == 0;
      // The CUE sheet can be read again, the CUE songs are restored, and the song for the whole media file is not used anymore.
      const bool cue_readable_again = new_cue_mtime != 0 && !cue_songs_in_db.isEmpty() && file_song_available;
      // No song is available, the CUE songs kept in the collection are not necessarily all the songs of the CUE sheet, for example when another CUE sheet was used for some of them, so the CUE sheet is loaded again instead of only restoring them.
      const bool cue_songs_unavailable = new_cue_mtime != 0 && !file_song_available && !cue_song_available;

      // LRC file with synchronized lyrics (if any), only used for media files without a CUE sheet.
      const QString lrc_file = new_cue_mtime == 0 ? lrc_files.value(NoExtensionPart(file)) : QString();
      const qint64 lrc_mtime = Utilities::FileMtimeMsec(lrc_file);

      // Watch out for CUE songs which have their mtime equal to qMax(media_file_mtime, cue_sheet_mtime)
      // The LRC file's mtime is stored separately, so an added, changed or deleted LRC file is detected regardless of how its mtime compares to the media file's.
      const bool changed_lrc = matching_song.lrc_mtime() != lrc_mtime;
      bool changed = (matching_song.mtime() != qMax(fileinfo.lastModified().toSecsSinceEpoch(), matching_song_cue_mtime)) || cue_deleted || cue_added || cue_changed || cue_readable_again || cue_songs_unavailable || changed_lrc;

      // Also want to look to see whether the album art has changed, the cover file's mtime is stored separately like for LRC files, so a cover file replaced with the same name is detected too.
      const QUrl art_automatic = ArtForSong(file, album_art);
      if (matching_song.art_automatic() != art_automatic || (!matching_song.art_automatic().isEmpty() && !matching_song.art_automatic_is_valid()) || matching_song.art_automatic_mtime() != ArtAutomaticMtime(art_automatic)) {
        changed = true;
      }

      bool missing_fingerprint = false;
      bool missing_loudness_characteristics = false;
#ifdef HAVE_SONGTRACKING
      if (song_tracking_ && matching_song.fingerprint().isEmpty()) {
        missing_fingerprint = true;
      }
#endif
#ifdef HAVE_EBUR128
      if (song_ebur128_loudness_analysis_ && (!matching_song.ebur128_integrated_loudness_lufs() || !matching_song.ebur128_loudness_range_lu())) {
        missing_loudness_characteristics = true;
      }
#endif

      // The added, changed and deleted songs are logged when the transaction is committed.
      if (!changed) {
        if (missing_fingerprint) {
          qLog(Debug) << file << "is missing fingerprint.";
        }
        else if (missing_loudness_characteristics) {
          qLog(Debug) << file << "is missing EBU R 128 loudness characteristics.";
        }
      }

      // If the song is unavailable and nothing has changed, just mark it as available without re-scanning
      // For CUE files with multiple sections, all sections share the same file and would have the same availability status
      if (matching_song.unavailable() && !changed && !missing_fingerprint && !missing_loudness_characteristics) {
        // Only the songs in use, not the songs which were used while the CUE sheet couldn't be read, or from another CUE sheet.
        t->readded_songs << songs_in_use;
      }
      // The song's changed or missing fingerprint - create fingerprint and reread the metadata from file.
      else if (t->ignores_mtime() || changed || missing_fingerprint || missing_loudness_characteristics) {

        QString fingerprint;
#ifdef HAVE_SONGTRACKING
        if (song_tracking_) {
          Chromaprinter chromaprinter(file);
          fingerprint = chromaprinter.CreateFingerprint();
          if (fingerprint.isEmpty()) {
            fingerprint = "NONE"_L1;
          }
        }
#endif

        if (new_cue.isEmpty() || new_cue_mtime == 0) {  // If no CUE or it's about to lose it.
          if (!UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, lrc_file, cue_deleted, t)) {
            file_valid = false;
          }
        }
        else {  // If CUE associated.
          SongList cue_songs;
          // When the CUE sheet can't be read now, it's handled on the next scan.
          if (LoadCueSongs(file, new_cue, path, &loaded_cue_sheets, &unreadable_cue_sheets, &t->directory_entries_, &cue_songs)) {
            if (cue_songs.isEmpty()) {  // The CUE sheet has no valid songs for the media file.
              if (!UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, lrc_files.value(NoExtensionPart(file)), !cue_songs_in_db.isEmpty(), t)) {
                file_valid = false;
              }
            }
            else {
              UpdateCueSongs(fingerprint, cue_songs, art_automatic, new_cue, matching_songs, t);
            }
          }
        }
      }

    }
    else {  // Search the DB by fingerprint.
      QString fingerprint;
#ifdef HAVE_SONGTRACKING
      if (song_tracking_) {
        Chromaprinter chromaprinter(file);
        fingerprint = chromaprinter.CreateFingerprint();
        if (fingerprint.isEmpty()) {
          fingerprint = "NONE"_L1;
        }
      }
#endif
      if (song_tracking_ && !fingerprint.isEmpty() && fingerprint != "NONE"_L1 && FindSongsByFingerprint(file, fingerprint, matching_songs)) {

        // The song is in the database and still on disk.
        // Check the mtime to see if it's been changed since it was added.
        const QFileInfo fileinfo(file);
        if (!fileinfo.exists()) {
          // Partially fixes race condition - if file was removed between being added to the list and now.
          t->AddToProgress(1);
          continue;
        }

        // The songs are updated with the new file path, they can also be in the deleted songs when the old file path is scanned in this transaction, that's resolved when the transaction is committed.
        bool matching_songs_has_cue = false;
        for (const Song &matching_song : std::as_const(matching_songs)) {
          if (matching_song.has_cue()) {
            matching_songs_has_cue = true;
          }
        }

        // CUE sheet's path from this file (if any).
        qint64 new_cue_mtime = 0;
        if (!new_cue.isEmpty()) {
          new_cue_mtime = static_cast<qint64>(GetMtimeForPath(new_cue));
        }

        // Get new album art
        const QUrl art_automatic = ArtForSong(file, album_art);

        if (new_cue.isEmpty() || new_cue_mtime == 0) {  // If no CUE or it's about to lose it.
          if (!UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, lrc_files.value(NoExtensionPart(file)), matching_songs_has_cue && new_cue_mtime == 0, t)) {
            file_valid = false;
          }
        }
        else {  // If CUE associated.
          SongList cue_songs;
          // When the CUE sheet can't be read, the songs are kept.
          if (LoadCueSongs(file, new_cue, path, &loaded_cue_sheets, &unreadable_cue_sheets, &t->directory_entries_, &cue_songs)) {
            if (cue_songs.isEmpty()) {  // The CUE sheet has no valid songs for the media file.
              if (!UpdateNonCueAssociatedSong(file, fingerprint, matching_songs, art_automatic, lrc_files.value(NoExtensionPart(file)), matching_songs_has_cue, t)) {
                file_valid = false;
              }
            }
            else {
              UpdateCueSongs(fingerprint, cue_songs, art_automatic, new_cue, matching_songs, t);
            }
          }
        }

      }
      else {  // The song is on disk but not in the DB

        // When the CUE sheet can't be read, the media file is added as a song for the whole media file, which is replaced by the CUE songs when it can be read.
        SongList cue_songs;
        if (!new_cue.isEmpty() && !LoadCueSongs(file, new_cue, path, &loaded_cue_sheets, &unreadable_cue_sheets, &t->directory_entries_, &cue_songs)) {
          cue_songs.clear();
        }

        const SongList songs = ScanNewFile(file, fingerprint, cue_songs, lrc_files.value(NoExtensionPart(file)));
        if (songs.isEmpty()) {
          t->AddToProgress(1);
          continue;
        }

        // Choose art for the song(s)
        const QUrl art_automatic = ArtForSong(file, album_art);

        for (Song song : songs) {
          song.set_directory_id(t->dir_id());
          if (song.art_automatic().isEmpty()) song.set_art_automatic(art_automatic);
          song.set_art_automatic_mtime(ArtAutomaticMtime(song.art_automatic()));
          t->new_songs << song;
        }
      }
    }
    if (file_valid) {
      valid_files_on_disk.insert(file);
    }
    t->AddToProgress(1);
  }

  // Look for deleted songs.
  // The media files hold the on-disk path spelling while the database stores its own; the two can differ purely by Unicode normalization form (NFC vs NFD).
  // A song is on disk when it was matched with a media file by FindSongsByPath(), which uses the songs with the exact path, otherwise another spelling of the same file, so the songs of a deleted file whose name only differs in normalization form from another file are deleted.
  // The media files which couldn't be read are not in valid_files_on_disk.
  for (const Song &song : std::as_const(songs_in_db)) {
    const bool on_disk = matched_song_files.contains(song.id()) && valid_files_on_disk.contains(matched_song_files.value(song.id()));
    if (!song.unavailable() && !on_disk) {
      t->deleted_songs << song;
    }
  }

  // Add, update or delete subdir
  CollectionSubdirectory updated_subdir;
  updated_subdir.directory_id = t->dir_id();
  updated_subdir.mtime = path_mtime;
  updated_subdir.path = path;

  if (!path_info.exists() && updated_subdir.path != dir.path) {
    t->deleted_subdirs << updated_subdir;
  }
  else if (subdir.directory_id == -1) {
    t->new_subdirs << updated_subdir;
  }
  else if (subdir.mtime != updated_subdir.mtime) {
    t->touched_subdirs << updated_subdir;
  }

  // Recurse into the new subdirs that we found
  for (const CollectionSubdirectory &my_new_subdir : std::as_const(my_new_subdirs)) {
    if (stop_or_abort_requested()) return;
    ScanSubdirectory(dir, my_new_subdir.path, my_new_subdir, 0, t, true);
  }

}

QString CollectionWatcher::PathKey(const QString &filename) {

  // Not normalized, so files which only differ in Unicode normalization form are different, the media filenames from the CUE sheets are resolved to the names on disk with OnDiskFilename().
  return QDir::cleanPath(filename);

}

QHash<QString, QString> CollectionWatcher::FindCueSheets(const QStringList &cue_files, const QString &path, QSet<QString> *unreadable_cue_sheets, QHash<QString, QStringList> *directory_entries) const {

  QHash<QString, QString> cue_sheets;
  QHash<QString, int> cue_sheet_priorities;

  QStringList sorted_cue_files = cue_files;
  std::sort(sorted_cue_files.begin(), sorted_cue_files.end());

  for (const QString &cue_filename : std::as_const(sorted_cue_files)) {

    if (stop_or_abort_requested()) break;

    QFile cue_file(cue_filename);
    if (!cue_file.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Could not open CUE file" << cue_filename << "for reading:" << cue_file.errorString();
      unreadable_cue_sheets->insert(PathKey(cue_filename));
      continue;
    }
    // Only read the FILE lines, the songs are only loaded when a media file needs them.
    const QStringList media_files = cue_parser_->MediaFiles(&cue_file, QDir(path));
    cue_file.close();

    for (const QString &cue_media_file : media_files) {
      // The FILE line can use another case than the media file on case-insensitive filesystems, the media file is looked up with the name on disk, with the spelling of the directory being scanned.
      const QString media_file = ScanPath(OnDiskFilename(cue_media_file, directory_entries), path, directory_entries);
      const QString key = PathKey(media_file);
      // When several CUE sheets have songs for the same media file, use the one named after the media file, like before CUE sheets with several media files were supported, otherwise the first one.
      const int priority = CueSheetPriority(media_file, cue_filename, directory_entries);
      if (cue_sheets.contains(key) && priority >= cue_sheet_priorities.value(key)) continue;
      cue_sheets.insert(key, cue_filename);
      cue_sheet_priorities.insert(key, priority);
    }

  }

  return cue_sheets;

}

QString CollectionWatcher::OnDiskFilename(const QString &filename, QHash<QString, QStringList> *directory_entries) {

  // When the file doesn't exist with this name, it's either missing, or a different file on a case-sensitive filesystem.
  if (!QFileInfo::exists(filename)) return filename;

  return OnDiskPath(QDir::cleanPath(filename), directory_entries);

}

QString CollectionWatcher::OnDiskPath(const QString &path, QHash<QString, QStringList> *directory_entries) {

  // Every component of the path is resolved, also the directories, for example an absolute path in a CUE sheet with a directory in another Unicode normalization form.
  const QFileInfo fileinfo(path);
  const QString name = fileinfo.fileName();
  const QString parent = fileinfo.absolutePath();
  if (name.isEmpty() || parent == path) return path;

  const QString parent_on_disk = QDir(parent).isRoot() ? parent : OnDiskPath(parent, directory_entries);
  const QString parent_prefix = parent_on_disk.endsWith(u'/') ? parent_on_disk : parent_on_disk + QLatin1Char('/');

  QHash<QString, QStringList>::const_iterator it = directory_entries->constFind(parent_on_disk);
  if (it == directory_entries->constEnd()) {
    it = directory_entries->insert(parent_on_disk, QDir(parent_on_disk).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));
  }
  const QStringList &entries = it.value();

  if (entries.contains(name)) return parent_prefix + name;

  // The entry exists with another case or Unicode normalization form, use the name on disk when there's exactly one, distinct entries are never merged.
  const QString name_nfc = name.normalized(QString::NormalizationForm_C);
  QString entry_on_disk;
  for (const QString &entry : entries) {
    if (entry.normalized(QString::NormalizationForm_C).compare(name_nfc, Qt::CaseInsensitive) == 0) {
      if (!entry_on_disk.isEmpty()) return parent_prefix + name;
      entry_on_disk = entry;
    }
  }

  return parent_prefix + (entry_on_disk.isEmpty() ? name : entry_on_disk);

}

QString CollectionWatcher::ScanPath(const QString &filename_on_disk, const QString &path, QHash<QString, QStringList> *directory_entries) {

  // The directory being scanned can be spelled differently than on disk, for example a collection directory added with another case on a case-insensitive filesystem.
  // The media files in it are scanned with that spelling, so the media files on disk use it too.
  const QString path_on_disk = OnDiskFilename(QDir::cleanPath(path), directory_entries);
  if (path_on_disk == path) return filename_on_disk;
  if (filename_on_disk == path_on_disk) return path;
  if (filename_on_disk.startsWith(path_on_disk + QLatin1Char('/'))) {
    return QDir::cleanPath(path) + filename_on_disk.mid(path_on_disk.length());
  }

  return filename_on_disk;

}

int CollectionWatcher::CueSheetPriority(const QString &media_file, const QString &cue_filename, QHash<QString, QStringList> *directory_entries) {

  // Like CueParser::FindCueFilename(), "track.flac.cue" is preferred over "track.cue", but the CUE sheets are compared directly, so a CUE sheet with an uppercase suffix, which is found on case-sensitive filesystems, is also preferred.
  // Normalized, so a media file and its CUE sheet with names in different Unicode normalization forms are compared correctly.
  const QFileInfo media_fileinfo(QDir::cleanPath(media_file).normalized(QString::NormalizationForm_C));
  const QFileInfo cue_fileinfo(QDir::cleanPath(cue_filename).normalized(QString::NormalizationForm_C));
  if (QDir::cleanPath(media_fileinfo.absolutePath()) != QDir::cleanPath(cue_fileinfo.absolutePath())) return 4;
  if (cue_fileinfo.suffix().compare("cue"_L1, Qt::CaseInsensitive) != 0) return 4;

  // On case-insensitive filesystems, a CUE sheet named after the media file in another case is also named after it, like with CueParser::FindCueFilename(), which uses QFileInfo::exists().
  // This is checked with the filesystem, so on case-sensitive filesystems, the names are still different, and an exact match is preferred.
  // The other spelling is the same file when it exists, but is not another file in the directory.
  const QString cue_stem = cue_fileinfo.completeBaseName();
  const QString cue_dir = QFileInfo(cue_filename).absolutePath();
  QHash<QString, QStringList>::const_iterator it = directory_entries->constFind(cue_dir);
  if (it == directory_entries->constEnd()) {
    it = directory_entries->insert(cue_dir, QDir(cue_dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));
  }
  const QStringList &entries = it.value();
  const auto named_after = [&cue_stem, &cue_dir, &cue_fileinfo, &entries](const QString &stem) {
    const QString name = stem + QLatin1Char('.') + cue_fileinfo.suffix();
    return cue_stem.compare(stem, Qt::CaseInsensitive) == 0 && !entries.contains(name) && QFileInfo::exists(cue_dir + QLatin1Char('/') + name);
  };

  const QString media_filename = media_fileinfo.fileName();
  const QString media_basename = media_filename.section(u'.', 0, -2);
  if (cue_stem == media_filename) return 0;
  if (named_after(media_filename)) return 1;
  if (cue_stem == media_basename) return 2;
  if (named_after(media_basename)) return 3;

  return 4;

}

bool CollectionWatcher::LoadCueSongs(const QString &file, const QString &cue_file, const QString &path, QHash<QString, QMap<QString, SongList>> *loaded_cue_sheets, QSet<QString> *unreadable_cue_sheets, QHash<QString, QStringList> *directory_entries, SongList *songs) const {

  const QString cue_key = PathKey(cue_file);
  if (unreadable_cue_sheets->contains(cue_key)) return false;

  QHash<QString, QMap<QString, SongList>>::const_iterator it = loaded_cue_sheets->constFind(cue_key);
  if (it == loaded_cue_sheets->constEnd()) {
    QFile file_cue(cue_file);
    if (!file_cue.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Could not open CUE file" << cue_file << "for reading:" << file_cue.errorString();
      unreadable_cue_sheets->insert(cue_key);
      return false;
    }
    // Media files which don't exist are not included, so invalid media doesn't get into the collection.
    const QMap<QString, SongList> songs_by_file = cue_parser_->LoadByFile(&file_cue, cue_file, QDir(path), false);
    file_cue.close();
    QMap<QString, SongList> songs_by_key;
    for (QMap<QString, SongList>::const_iterator file_it = songs_by_file.constBegin(); file_it != songs_by_file.constEnd(); ++file_it) {
      // Like in FindCueSheets(), with the name of the media file on disk, which is also used for the URL of the songs, so they match the media file in the collection.
      const QString media_file = ScanPath(OnDiskFilename(file_it.key(), directory_entries), path, directory_entries);
      SongList media_file_songs = file_it.value();
      if (media_file != file_it.key()) {
        for (Song &song : media_file_songs) {
          song.set_url(QUrl::fromLocalFile(media_file));
        }
      }
      songs_by_key[PathKey(media_file)] << media_file_songs;
    }
    it = loaded_cue_sheets->insert(cue_key, songs_by_key);
  }

  *songs = it.value().value(PathKey(file));

  return true;

}

void CollectionWatcher::UpdateCueAssociatedSongs(const QString &fingerprint,
                                                 const SongList &cue_songs,
                                                 const QUrl &art_automatic,
                                                 const SongList &old_cue_songs,
                                                 ScanTransaction *t) const {

  QHash<quint64, Song> sections_map;
  for (const Song &song : old_cue_songs) {
    sections_map.insert(static_cast<quint64>(song.beginning_nanosec()), song);
  }

  // Update every song that's in the CUE and collection, the CUE songs are only the songs of this media file.
  QSet<int> used_ids;
  for (Song new_cue_song : cue_songs) {
    new_cue_song.set_source(source_);
    new_cue_song.set_directory_id(t->dir_id());
    PerformEBUR128Analysis(new_cue_song);
    new_cue_song.set_fingerprint(fingerprint);

    if (sections_map.contains(static_cast<quint64>(new_cue_song.beginning_nanosec()))) {  // Changed section
      const Song matching_cue_song = sections_map[static_cast<quint64>(new_cue_song.beginning_nanosec())];
      new_cue_song.set_id(matching_cue_song.id());
      new_cue_song.set_art_automatic(art_automatic);
      new_cue_song.set_art_automatic_mtime(ArtAutomaticMtime(art_automatic));
      new_cue_song.MergeUserSetData(matching_cue_song, true, true);
      SetLyricsFromStorage(&new_cue_song, matching_cue_song, QString());
      AddChangedSong(matching_cue_song, new_cue_song, t);
      used_ids.insert(matching_cue_song.id());
    }
    else {  // A new section, the album art is set like for new songs in the initial scan.
      if (new_cue_song.art_automatic().isEmpty()) new_cue_song.set_art_automatic(art_automatic);
      new_cue_song.set_art_automatic_mtime(ArtAutomaticMtime(new_cue_song.art_automatic()));
      t->new_songs << new_cue_song;
    }
  }

  // Sections that are now missing, the unavailable ones are not deleted again.
  for (const Song &old_cue : old_cue_songs) {
    if (!used_ids.contains(old_cue.id()) && !old_cue.unavailable()) {
      t->deleted_songs << old_cue;
    }
  }

}

void CollectionWatcher::UpdateCueSongs(const QString &fingerprint, const SongList &cue_songs, const QUrl &art_automatic, const QString &cue_file, const SongList &matching_songs, ScanTransaction *t) const {

  // The songs to update with the CUE songs, by their position, only CUE songs, the song for the whole media file is never used for a CUE song, so new CUE songs are added as new songs.
  // The unavailable CUE songs of the previous CUE sheet, for example when the media file was restored with a renamed CUE sheet, when the CUE sheet was renamed while it couldn't be read, or when the media file was moved and found by fingerprint after the old location was scanned.
  // Then the available CUE songs from another CUE sheet, then the CUE songs of this CUE sheet, which are used before another song at the same position, so the songs keep their statistics when switching between CUE sheets.
  const QString cue_key = PathKey(cue_file);
  SongList available_other_cue_songs;
  SongList cue_file_songs;
  QHash<QString, SongList> unavailable_other_cue_songs;
  QHash<QString, qint64> unavailable_other_cue_lastseen;
  for (const Song &song : matching_songs) {
    if (!song.has_cue()) {
      continue;
    }
    else if (PathKey(song.cue_path()) == cue_key) {
      cue_file_songs << song;
    }
    else if (!song.unavailable()) {
      available_other_cue_songs << song;
    }
    else {
      const QString other_cue_key = PathKey(song.cue_path());
      unavailable_other_cue_songs[other_cue_key] << song;
      unavailable_other_cue_lastseen[other_cue_key] = qMax(unavailable_other_cue_lastseen.value(other_cue_key, -1), song.lastseen());
    }
  }

  // The previous CUE sheet is the one which was seen last, not older CUE sheets which were replaced before.
  QString previous_cue_key;
  for (QHash<QString, qint64>::const_iterator it = unavailable_other_cue_lastseen.constBegin(); it != unavailable_other_cue_lastseen.constEnd(); ++it) {
    if (previous_cue_key.isEmpty() || it.value() > unavailable_other_cue_lastseen.value(previous_cue_key) || (it.value() == unavailable_other_cue_lastseen.value(previous_cue_key) && it.key() < previous_cue_key)) {
      previous_cue_key = it.key();
    }
  }

  // UpdateCueAssociatedSongs() uses the last song at the same position, so the CUE songs of the previous CUE sheet have the lowest priority.
  // They are used for positions which the other songs don't have, for example when the kept CUE songs of this CUE sheet are only some of its songs, since another CUE sheet was used for some of them.
  const SongList old_songs = unavailable_other_cue_songs.value(previous_cue_key) + available_other_cue_songs + cue_file_songs;

  UpdateCueAssociatedSongs(fingerprint, cue_songs, art_automatic, old_songs, t);

  // The other available songs are not used anymore, for example the song for the whole media file, which is marked unavailable or deleted.
  QSet<int> old_song_ids;
  for (const Song &song : old_songs) {
    old_song_ids.insert(song.id());
  }
  for (const Song &song : matching_songs) {
    if (!song.unavailable() && !old_song_ids.contains(song.id())) {
      t->deleted_songs << song;
    }
  }

}

void CollectionWatcher::UseMediaFileWhileCueSheetUnreadable(const QString &file, const QFileInfo &fileinfo, const SongList &cue_songs_in_db, const SongList &file_songs_in_db, const QUrl &art_automatic, const QString &lrc_file, ScanTransaction *t) {

  // Mark the CUE songs unavailable, they are restored when the CUE sheet can be read again.
  bool cue_songs_available = false;
  for (const Song &cue_song : cue_songs_in_db) {
    if (!cue_song.unavailable()) {
      t->deleted_songs << cue_song;
      cue_songs_available = true;
    }
  }
  if (cue_songs_available) {
    qLog(Warning) << "The CUE sheet" << cue_songs_in_db.first().cue_path() << "can't be read, using" << file << "as a single song until it can be read";
  }

  // The song for the whole media file is already used, only update it when the media file, the LRC file or the album art changed, like in the normal scan.
  const Song file_song_in_db = file_songs_in_db.isEmpty() ? Song() : file_songs_in_db.first();
  if (file_song_in_db.is_valid() && !t->ignores_mtime() && !file_song_in_db.unavailable() && file_song_in_db.mtime() == fileinfo.lastModified().toSecsSinceEpoch()) {
    const bool changed_lrc = file_song_in_db.lrc_mtime() != Utilities::FileMtimeMsec(lrc_file);
    const bool changed_art = file_song_in_db.art_automatic() != art_automatic || (!file_song_in_db.art_automatic().isEmpty() && !file_song_in_db.art_automatic_is_valid()) || file_song_in_db.art_automatic_mtime() != ArtAutomaticMtime(art_automatic);
    bool missing_fingerprint = false;
    bool missing_loudness_characteristics = false;
#ifdef HAVE_SONGTRACKING
    missing_fingerprint = song_tracking_ && file_song_in_db.fingerprint().isEmpty();
#endif
#ifdef HAVE_EBUR128
    missing_loudness_characteristics = song_ebur128_loudness_analysis_ && (!file_song_in_db.ebur128_integrated_loudness_lufs() || !file_song_in_db.ebur128_loudness_range_lu());
#endif
    if (!changed_lrc && !changed_art && !missing_fingerprint && !missing_loudness_characteristics) {
      return;
    }
  }

  QString fingerprint;
#ifdef HAVE_SONGTRACKING
  if (song_tracking_) {
    Chromaprinter chromaprinter(file);
    fingerprint = chromaprinter.CreateFingerprint();
    if (fingerprint.isEmpty()) {
      fingerprint = "NONE"_L1;
    }
  }
#endif

  if (file_song_in_db.is_valid()) {
    // The media file can't be read either, retire the song for the whole media file, like the normal scan does.
    if (!UpdateNonCueAssociatedSong(file, fingerprint, file_songs_in_db, art_automatic, lrc_file, false, t)) {
      for (const Song &file_song : file_songs_in_db) {
        if (!file_song.unavailable()) {
          t->deleted_songs << file_song;
        }
      }
    }
    return;
  }

  // Add the media file as a separate song.
  const SongList songs = ScanNewFile(file, fingerprint, SongList(), lrc_file);
  for (Song song : songs) {
    song.set_directory_id(t->dir_id());
    if (song.art_automatic().isEmpty()) song.set_art_automatic(art_automatic);
    song.set_art_automatic_mtime(ArtAutomaticMtime(song.art_automatic()));
    t->new_songs << song;
  }

}

bool CollectionWatcher::UpdateNonCueAssociatedSong(const QString &file,
                                                   const QString &fingerprint,
                                                   const SongList &matching_songs,
                                                   const QUrl &art_automatic,
                                                   const QString &lrc_file,
                                                   const bool cue_deleted,
                                                   ScanTransaction *t) {

  // The song for the whole media file, a CUE song is never used for it, so when the CUE sheet is deleted, the media file is added as a new song, unless there already is a song for the whole media file, for example from when the CUE sheet couldn't be read.
  Song matching_song;
  for (const Song &song : matching_songs) {
    if (!song.has_cue()) {
      matching_song = song;
      break;
    }
  }

  // If the CUE sheet was deleted, the CUE songs are marked unavailable or deleted.
  if (cue_deleted) {
    for (const Song &song : matching_songs) {
      if (song.has_cue() && !song.unavailable()) {
        t->deleted_songs << song;
      }
    }
  }

  Song song_on_disk(source_);
  const TagReaderResult result = tagreader_client_->ReadFileBlocking(file, &song_on_disk);
  if (result.success() && song_on_disk.is_valid()) {
    song_on_disk.set_source(source_);
    song_on_disk.set_directory_id(t->dir_id());
    song_on_disk.set_id(matching_song.id());
    PerformEBUR128Analysis(song_on_disk);
    song_on_disk.set_fingerprint(fingerprint);
    song_on_disk.set_art_automatic(art_automatic);
    song_on_disk.set_art_automatic_mtime(ArtAutomaticMtime(art_automatic));
    SetLyricsFromStorage(&song_on_disk, matching_song, lrc_file);
    if (matching_song.is_valid()) {
      song_on_disk.MergeUserSetData(matching_song, !overwrite_playcount_, !overwrite_rating_);
      AddChangedSong(matching_song, song_on_disk, t);
    }
    else {
      t->new_songs << song_on_disk;
    }
  }

  return result.success() && song_on_disk.is_valid();

}

SongList CollectionWatcher::ScanNewFile(const QString &file, const QString &fingerprint, const SongList &cue_songs, const QString &lrc_file) const {

  SongList songs;

  if (!cue_songs.isEmpty()) {  // If it's a CUE - create virtual tracks, the CUE songs are only the songs of this media file.
    songs.reserve(cue_songs.count());
    for (Song cue_song : cue_songs) {
      cue_song.set_source(source_);
      PerformEBUR128Analysis(cue_song);
      cue_song.set_fingerprint(fingerprint);
      songs << cue_song;
    }
  }
  else {  // It's a normal media file
    Song song(source_);
    const TagReaderResult result = tagreader_client_->ReadFileBlocking(file, &song);
    if (result.success() && song.is_valid()) {
      song.set_source(source_);
      PerformEBUR128Analysis(song);
      song.set_fingerprint(fingerprint);
      SetLyricsFromStorage(&song, Song(), lrc_file);
      songs << song;
    }
  }

  return songs;

}

void CollectionWatcher::AddChangedSong(const Song &matching_song, const Song &new_song, ScanTransaction *t) {

  bool notify_new = false;
  QStringList changes;

  if (matching_song.unavailable()) {
    changes << u"restored"_s;
    notify_new = true;
  }
  else {
    if (matching_song.url() != new_song.url()) {
      changes << u"file path from %1"_s.arg(matching_song.url().toLocalFile());
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
    // Touched songs only have their mtime updated, so where the lyrics are stored, including the LRC file's mtime, needs a full update to be stored, also when the lyrics did not change.
    if (!matching_song.IsLyricsStorageEqual(new_song)) {
      changes << u"lyrics storage"_s;
      notify_new = true;
    }

  }

  // The changes are logged when the transaction is committed.
  if (!changes.isEmpty()) {
    t->song_changes.insert(new_song.id(), changes);
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

quint64 CollectionWatcher::GetMtimeForPath(const QString &path) {

  if (path.isEmpty()) {
    return 0;
  }

  const QFileInfo fileinfo(path);
  if (!fileinfo.exists()) {
    return 0;
  }

  const QDateTime last_modified = fileinfo.lastModified();

  return last_modified.isValid() ? static_cast<quint64>(last_modified.toSecsSinceEpoch()) : 0;

}

QString CollectionWatcher::ReadSyncedLyricsFromLrc(const QString &lrc_file) {

  QFile file(lrc_file);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open LRC file" << lrc_file << "for reading:" << file.errorString();
    return QString();
  }

  // Read at most one byte more than the limit, so the limit holds even if the file grows after its size was checked.
  const QByteArray data = file.read(kMaxLrcFileSize + 1);
  file.close();
  if (data.size() > kMaxLrcFileSize) {
    qLog(Warning) << "Ignoring LRC file" << lrc_file << "which is larger than" << kMaxLrcFileSize << "bytes";
    return QString();
  }

  // LRC files are not always UTF-8, detect the encoding like for other text files.
  const QString synced_lyrics = Utilities::TextFromData(data);
  const std::optional<Utilities::LRCLines> lrc_lines = Utilities::ParseLRC(synced_lyrics);
  if (!lrc_lines.has_value() || lrc_lines->isEmpty()) {
    qLog(Debug) << "Ignoring LRC file" << lrc_file << "which is not valid LRC";
    return QString();
  }

  return synced_lyrics;

}

void CollectionWatcher::SetLyricsFromStorage(Song *song, const Song &matching_song, const QString &lrc_file) {

  // song has just been read from the tags, which sets the storage type to the tags for lyrics found there, otherwise it is the database.
  // matching_song is the song from the collection, or an empty song for new songs.

  // Lyrics stored in the database only are kept.
  // Empty lyrics in the database are lyrics cleared on purpose, so they are kept too, otherwise they would be restored from the tags or an LRC file.
  if (matching_song.is_valid() && matching_song.lyrics_storage_type() == Song::StorageType::Database) {
    song->set_lyrics(matching_song.lyrics());
    song->set_lyrics_storage_type(Song::StorageType::Database);
  }

  // Store the LRC file's mtime even if it is not used or not valid, so the song is only rescanned again when the LRC file changes.
  const QFileInfo lrc_fileinfo(lrc_file);
  const bool lrc_file_exists = !lrc_file.isEmpty() && lrc_fileinfo.exists();
  song->set_lrc_mtime(lrc_file_exists ? Utilities::FileMtimeMsec(lrc_file) : 0);

  if (matching_song.is_valid() && matching_song.synced_lyrics_storage_type() == Song::StorageType::Database) {
    song->set_synced_lyrics(matching_song.synced_lyrics());
    song->set_synced_lyrics_storage_type(Song::StorageType::Database);
    return;
  }

  // Synchronized lyrics saved to an LRC file take precedence over the tags, otherwise the tags take precedence and the LRC file is only used when the tags have none.
  if (!lrc_file_exists || (matching_song.synced_lyrics_storage_type() != Song::StorageType::File && !song->synced_lyrics().isEmpty())) {
    return;
  }

  // Skip reading larger files completely, ReadSyncedLyricsFromLrc() also limits how much it reads.
  if (lrc_fileinfo.size() > kMaxLrcFileSize) {
    qLog(Warning) << "Ignoring LRC file" << lrc_file << "which is larger than" << kMaxLrcFileSize << "bytes";
    return;
  }

  const QString synced_lyrics = ReadSyncedLyricsFromLrc(lrc_file);
  if (!synced_lyrics.isEmpty()) {
    song->set_synced_lyrics(synced_lyrics);
    song->set_synced_lyrics_storage_type(Song::StorageType::File);
  }

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

QHash<QString, SongList> CollectionWatcher::SongsByPath(const SongList &songs) {

  // Keyed on the NFC-normalized path so a file that differs only in Unicode normalization (NFC vs NFD) between disk and database still matches, instead of being treated as a new file and duplicated.
  // The songs for each path keep the order of the list, several songs can share a path with a CUE sheet.
  QHash<QString, SongList> songs_by_path;
  songs_by_path.reserve(songs.count());
  for (const Song &song : songs) {
    songs_by_path[song.url().toLocalFile().normalized(QString::NormalizationForm_C)] << song;
  }

  return songs_by_path;

}

bool CollectionWatcher::FindSongsByPath(const QHash<QString, SongList> &songs_by_path, const QString &path, const QSet<QString> &files_on_disk, SongList &songs_out) {

  // The songs with the exact path are used when there are any.
  // Otherwise a song with another spelling of the path is the same file when that spelling is not another file on disk, for example when the database and the filesystem use different Unicode normalization forms.
  const SongList songs = songs_by_path.value(path.normalized(QString::NormalizationForm_C));
  SongList other_spelling_songs;
  for (const Song &song : songs) {
    const QString song_file = song.url().toLocalFile();
    if (song_file == path) {
      songs_out << song;
    }
    else if (!files_on_disk.contains(song_file)) {
      other_spelling_songs << song;
    }
  }
  if (songs_out.isEmpty()) {
    songs_out << other_spelling_songs;
  }

  return !songs_out.isEmpty();

}

bool CollectionWatcher::FindSongsByFingerprint(const QString &file, const QString &fingerprint, SongList &songs_out) {

  const SongList songs = backend_->GetSongsByFingerprint(fingerprint);
  for (const Song &song : songs) {
    QString filename = song.url().toLocalFile();
    QFileInfo info(filename);
    // Allow multiple songs in different directories with the same fingerprint.
    // Only use the matching song by fingerprint if it doesn't already exist in a different path.
    if (file == filename || !info.exists()) {
      songs_out << song;
    }
  }

  return !songs_out.isEmpty();

}

bool CollectionWatcher::FindSongsByFingerprint(const QString &file, const SongList &songs, const QString &fingerprint, SongList &songs_out) {

  for (const Song &song : songs) {
    QString filename = song.url().toLocalFile();
    if (song.fingerprint() == fingerprint && (file == filename || !QFileInfo::exists(filename))) {
      songs_out << song;
      return true;
    }
  }

  return !songs_out.isEmpty();

}

void CollectionWatcher::DirectoryChanged(const QString &subdir) {

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

void CollectionWatcher::FileSystemWatcherOverflow() {

  // Changes were lost, so new directories and files would be missed until the next scan, find them by scanning all directories.
  qLog(Debug) << "Changes were lost, scanning the collection";

  RestoreWatches();

  overflow_scan_pending_ = true;
  if (!rescan_paused_) rescan_timer_->start();

}

void CollectionWatcher::RestoreWatches() {

  if (!monitor_) return;

  // A watched directory which was deleted and created again lost its watch, but the removal of the watch was lost too, so it's still registered, add all the watches again.
  // The watches are not removed first, removing them would give an event for each of them, which could overflow the event queue again.
  QStringList existing_paths;
  QStringList removed_paths;
  const QStringList paths = subdir_mapping_.keys();
  for (const QString &path : paths) {
    if (QFile::exists(path)) {
      existing_paths << path;
    }
    else {
      removed_paths << path;
    }
  }
  fs_watcher_->RemovePaths(removed_paths);
  fs_watcher_->AddPaths(existing_paths);

}

void CollectionWatcher::RescanPathsNow() {

  // Stopping a scan only stops the scan which was running, otherwise all the changed paths queued after it are thrown away without being scanned.
  CancelStop();

  // The scan for the overflow doesn't update when the songs were last seen, the songs which are still available are seen now, otherwise a song marked unavailable by the scans could be expired right away, because it was last seen long ago.
  // This is done before the changed paths are rescanned, they can also mark songs unavailable.
  const bool overflow_scan = overflow_scan_pending_ && !rescan_paused_;
  if (overflow_scan) {
    // A directory which didn't exist when the file system watcher overflowed could have been created again since then, the scan only adds watches for new directories.
    RestoreWatches();
    for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
      Q_EMIT UpdateLastSeen(dir.id, 0);
    }
  }

  const QList<int> dir_ids = rescan_queue_.keys();
  for (const int dir_id : dir_ids) {

    if (stop_or_abort_requested()) break;
    ScanTransaction transaction(this, dir_id, false, false);

    const QStringList paths = rescan_queue_.value(dir_id);

    QMap<QString, quint64> subdir_files_count;
    for (const QString &path : paths) {
      const quint64 files_count = FilesCountForPath(&transaction, path);
      subdir_files_count[path] = files_count;
      transaction.AddToProgressMax(files_count);
    }

    for (const QString &path : paths) {
      if (stop_or_abort_requested()) break;
      if (!subdir_mapping_.contains(path)) {
        continue;
      }
      CollectionSubdirectory subdir;
      subdir.directory_id = dir_id;
      subdir.mtime = 0;
      subdir.path = path;
      ScanSubdirectory(subdir_mapping_[path], path, subdir, subdir_files_count[path], &transaction);
    }
  }

  rescan_queue_.clear();

  Q_EMIT CompilationsNeedUpdating();

  // Scan all directories to find the changes which were lost when the file system watcher overflowed.
  // Not an incremental scan, which skips directories with an unchanged modification time, so changes to files in them would be missed, the files are still only read again when they changed.
  // The rescan timer could already be running when the rescan was paused, then the scan is done when it's resumed.
  if (overflow_scan && !stop_or_abort_requested()) {
    PerformScan(false, false);
    // When the scan was stopped, not all directories were scanned, so it's done again with the next rescan.
    overflow_scan_pending_ = stop_or_abort_requested();
    // The scan doesn't update when the songs were last seen or expire unavailable songs, like the incremental scan does, but it counts as the last scan, so the periodic scan which does is not done for a day.
    // Songs which were available again were not updated before the scan either.
    if (!overflow_scan_pending_) {
      for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
        Q_EMIT UpdateLastSeen(dir.id, expire_unavailable_songs_days_);
      }
    }
  }

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

qint64 CollectionWatcher::ArtAutomaticMtime(const QUrl &art_automatic) {

  return art_automatic.isLocalFile() ? Utilities::FileMtimeMsec(art_automatic.toLocalFile()) : 0;

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
  if (!rescan_paused_ && (!rescan_queue_.isEmpty() || overflow_scan_pending_)) RescanPathsNow();

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

    ScanTransaction transaction(this, dir.id, incremental, ignore_mtimes);
    CollectionSubdirectoryList subdirs = transaction.GetAllSubdirs();

    const bool has_collection_root_dir = std::any_of(subdirs.begin(), subdirs.end(), [&dir](const CollectionSubdirectory &subdir) { return subdir.path == dir.path; });
    if (!has_collection_root_dir) {
      qLog(Debug) << "Collection directory wasn't in subdir list, re-adding";
      CollectionSubdirectory subdir;
      subdir.directory_id = dir.id;
      subdir.path = dir.path;
      subdir.mtime = 0;
      subdirs << subdir;
    }

    QMap<QString, quint64> subdir_files_count;
    quint64 files_count = FilesCountForSubdirs(&transaction, subdirs, subdir_files_count);
    transaction.AddToProgressMax(files_count);

    for (const CollectionSubdirectory &subdir : std::as_const(subdirs)) {
      if (stop_or_abort_requested()) break;
      ScanSubdirectory(dir, subdir.path, subdir, subdir_files_count[subdir.path], &transaction);
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
      if (real_path == dir.path || real_path.startsWith(dir.path + u'/')) {
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

        bool points_into_watched_dir = false;
        for (const CollectionDirectory &dir : std::as_const(watched_dirs_)) {
          if (real_path == dir.path || real_path.startsWith(dir.path + u'/')) {
            points_into_watched_dir = true;
            break;
          }
        }
        // A symlink into an already-watched directory would recurse/double-count; skip it.
        if (points_into_watched_dir) continue;
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
    if (!watched_dirs_.contains(song.directory_id())) continue;
    const CollectionDirectory dir = watched_dirs_.value(song.directory_id());
    const QString song_path = song.url().toLocalFile().section(u'/', 0, -2);
    if (scanned_paths.contains(song_path)) continue;
    ScanTransaction transaction(this, song.directory_id(), false, true);
    const CollectionSubdirectoryList subdirs = transaction.GetAllSubdirs();
    for (const CollectionSubdirectory &subdir : subdirs) {
      if (stop_or_abort_requested()) break;
      if (subdir.path != song_path) continue;
      qLog(Debug) << "Rescan for directory ID" << song.directory_id() << "directory" << subdir.path;
      const quint64 files_count = FilesCountForPath(&transaction, subdir.path);
      ScanSubdirectory(dir, song_path, subdir, files_count, &transaction);
      scanned_paths << subdir.path;
    }
  }

  Q_EMIT CompilationsNeedUpdating();

}
