/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <cmath>

#include <gst/gst.h>

#include "gtest_include.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QIODevice>
#include <QMap>
#include <QSignalSpy>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "collection/collectiondirectory.h"
#include "collection/collectionwatcher.h"
#include "constants/collectionsettings.h"
#include "core/settings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class CollectionWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // For the fingerprints with Chromaprint.
    gst_init(nullptr, nullptr);
    tagreader_client_thread_ = new QThread();
    tagreader_client_ = SharedPtr<TagReaderClient>(new TagReaderClient(), [](TagReaderClient *tagreader_client) { tagreader_client->deleteLater(); });
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();

    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  }

  void TearDown() override {
    backend_.reset();
    database_.reset();
    tagreader_client_.reset();
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  // Scans the directory like a newly added collection directory, and returns the songs found.
  SongList ScanDirectory(const QString &path) {
    CollectionWatcher watcher(Song::Source::Collection, task_manager_, tagreader_client_, backend_);
    QSignalSpy spy(&watcher, &CollectionWatcher::NewOrUpdatedSongs);
    CollectionDirectory dir;
    dir.id = 1;
    dir.path = path;
    watcher.AddDirectory(dir, CollectionSubdirectoryList());
    SongList songs;
    for (const QList<QVariant> &arguments : std::as_const(spy)) {
      songs << arguments.at(0).value<SongList>();
    }
    return songs;
  }

  // Scans the directory like the collection does, storing the songs in the collection backend.
  // Returns the number of songs which were added or updated, deleted, marked unavailable and rescanned without changes.
  struct ScanResult {
    int new_or_updated = 0;
    int deleted = 0;
    int unavailable = 0;
    int touched = 0;
    int readded = 0;
  };
  // With subdirectories, only those are scanned, in that order, in one transaction, like the incremental scan on startup, their mtime is not set, so they are always scanned.
  ScanResult ScanDirectoryToCollection(const QString &path, const bool song_tracking = false, const QStringList &subdirectories = QStringList()) {
    CollectionWatcher watcher(Song::Source::Collection, task_manager_, tagreader_client_, backend_);
    if (song_tracking) {
      // The settings are in a temporary directory for the tests, the setting is removed again after the scan.
      Settings s;
      s.beginGroup(CollectionSettings::kSettingsGroup);
      s.setValue(CollectionSettings::kSongTracking, true);
      s.endGroup();
      watcher.ReloadSettingsAsync();
      QCoreApplication::processEvents();
    }
    QObject::connect(&watcher, &CollectionWatcher::NewOrUpdatedSongs, &*backend_, &CollectionBackend::AddOrUpdateSongs);
    QObject::connect(&watcher, &CollectionWatcher::SongsMTimeUpdated, &*backend_, &CollectionBackend::UpdateMTimesOnly);
    QObject::connect(&watcher, &CollectionWatcher::SongsDeleted, &*backend_, &CollectionBackend::DeleteSongs);
    QObject::connect(&watcher, &CollectionWatcher::SongsUnavailable, &*backend_, &CollectionBackend::MarkSongsUnavailable);
    // Like CollectionManager, songs which are restored without changes are marked available again.
    QObject::connect(&watcher, &CollectionWatcher::SongsReadded, &*backend_, &CollectionBackend::MarkSongsUnavailable);
    QObject::connect(&watcher, &CollectionWatcher::SubdirsDiscovered, &*backend_, &CollectionBackend::AddOrUpdateSubdirs);
    QSignalSpy spy_new_or_updated(&watcher, &CollectionWatcher::NewOrUpdatedSongs);
    QSignalSpy spy_deleted(&watcher, &CollectionWatcher::SongsDeleted);
    QSignalSpy spy_unavailable(&watcher, &CollectionWatcher::SongsUnavailable);
    QSignalSpy spy_touched(&watcher, &CollectionWatcher::SongsMTimeUpdated);
    QSignalSpy spy_readded(&watcher, &CollectionWatcher::SongsReadded);
    CollectionDirectory dir;
    dir.id = 1;
    dir.path = path;
    CollectionSubdirectoryList subdirs;
    for (const QString &subdirectory : subdirectories) {
      CollectionSubdirectory subdir;
      subdir.directory_id = dir.id;
      subdir.path = subdirectory;
      subdirs << subdir;
    }
    watcher.AddDirectory(dir, subdirs);
    ScanResult result;
    for (const QList<QVariant> &arguments : std::as_const(spy_new_or_updated)) result.new_or_updated += static_cast<int>(arguments.at(0).value<SongList>().count());
    for (const QList<QVariant> &arguments : std::as_const(spy_deleted)) result.deleted += static_cast<int>(arguments.at(0).value<SongList>().count());
    for (const QList<QVariant> &arguments : std::as_const(spy_unavailable)) result.unavailable += static_cast<int>(arguments.at(0).value<SongList>().count());
    for (const QList<QVariant> &arguments : std::as_const(spy_touched)) result.touched += static_cast<int>(arguments.at(0).value<SongList>().count());
    for (const QList<QVariant> &arguments : std::as_const(spy_readded)) result.readded += static_cast<int>(arguments.at(0).value<SongList>().count());
    if (song_tracking) {
      Settings s;
      s.beginGroup(CollectionSettings::kSettingsGroup);
      s.remove(CollectionSettings::kSongTracking);
      s.endGroup();
    }
    return result;
  }

  // Creates the directory with CD1.mp3, CD2.mp3 and Album.cue with songs for both media files.
  static void CreateAlbumWithCueSheet(const QTemporaryDir &temp_dir) {
    for (const QString &filename : QStringList{u"CD1.mp3"_s, u"CD2.mp3"_s}) {
      ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(filename)));
      // Files copied from the resources are read-only.
      ASSERT_TRUE(QFile::setPermissions(temp_dir.filePath(filename), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    }
    QByteArray cue;
    cue += "PERFORMER \"Artist\"\n";
    cue += "TITLE \"Album\"\n";
    cue += "FILE \"CD1.mp3\" MP3\n";
    cue += "  TRACK 01 AUDIO\n";
    cue += "    TITLE \"First\"\n";
    cue += "    INDEX 01 00:00:00\n";
    cue += "  TRACK 02 AUDIO\n";
    cue += "    TITLE \"Second\"\n";
    cue += "    INDEX 01 00:01:00\n";
    cue += "FILE \"CD2.mp3\" MP3\n";
    cue += "  TRACK 03 AUDIO\n";
    cue += "    TITLE \"Third\"\n";
    cue += "    INDEX 01 00:00:00\n";
    QFile cue_file(temp_dir.filePath(u"Album.cue"_s));
    ASSERT_TRUE(cue_file.open(QIODevice::WriteOnly));
    ASSERT_EQ(cue.size(), cue_file.write(cue));
    cue_file.close();
  }

  // Returns the titles of the available songs, with the CUE sheet path, and the number of unavailable songs.
  QStringList AvailableSongs(int *unavailable = nullptr) const {
    QStringList songs;
    int unavailable_songs = 0;
    const SongList all_songs = backend_->GetAllSongs();
    for (const Song &song : all_songs) {
      if (song.unavailable()) {
        ++unavailable_songs;
      }
      else {
        songs << song.title() + u'|' + song.cue_path();
      }
    }
    songs.sort();
    if (unavailable) *unavailable = unavailable_songs;
    return songs;
  }

  static void WriteFile(const QString &filename, const QByteArray &data) {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(data.size(), file.write(data));
    file.close();
  }

  // Makes the file unreadable, returns false when it can still be read, for example when the test is running as root.
  static bool MakeUnreadable(const QString &filename) {
    if (!QFile::setPermissions(filename, QFile::Permissions())) return false;
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
      MakeReadable(filename);
      return false;
    }
    return true;
  }

  // Writes a mono 16-bit WAV file with tones changing every 100 ms, long enough for a fingerprint, the seed makes a different file.
  static void WriteWavFile(const QString &filename, const quint32 seed, const int seconds = 20) {
    constexpr int sample_rate = 44100;
    const quint32 samples = static_cast<quint32>(sample_rate * seconds);
    QByteArray data;
    data.reserve(static_cast<qsizetype>(samples * 2 + 44));
    const auto append_u32 = [&data](const quint32 value) { for (int i = 0; i < 4; ++i) data.append(static_cast<char>((value >> (i * 8)) & 0xFF)); };
    const auto append_u16 = [&data](const quint16 value) { data.append(static_cast<char>(value & 0xFF)); data.append(static_cast<char>((value >> 8) & 0xFF)); };
    data.append("RIFF");
    append_u32(36 + samples * 2);
    data.append("WAVEfmt ");
    append_u32(16);
    append_u16(1);
    append_u16(1);
    append_u32(sample_rate);
    append_u32(sample_rate * 2);
    append_u16(2);
    append_u16(16);
    data.append("data");
    append_u32(samples * 2);
    quint32 state = seed;
    double frequency = 440.0;
    double phase = 0.0;
    for (quint32 i = 0; i < samples; ++i) {
      if (i % (sample_rate / 10) == 0) {
        state = state * 1664525U + 1013904223U;
        frequency = 200.0 + static_cast<double>(state % 2000U);
      }
      phase += 2.0 * M_PI * frequency / sample_rate;
      append_u16(static_cast<quint16>(static_cast<qint16>(std::sin(phase) * 12000.0)));
    }
    WriteFile(filename, data);
  }

  static void SetModificationTime(const QString &filename, const QDateTime &mtime) {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.setFileTime(mtime, QFileDevice::FileModificationTime));
    file.close();
  }

  static void MakeReadable(const QString &filename) {
    QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
  }

  QThread *tagreader_client_thread_ = nullptr;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> backend_;
};

// A CUE sheet with several FILE sections, which is not named after the media files, gives each media file only its own songs.
TEST_F(CollectionWatcherTest, CueSheetWithSeveralMediaFiles) {

  // Not in the temporary directory, which is often a tmpfs, which the collection watcher ignores.
  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;

  const SongList songs = ScanDirectory(temp_dir.path());

  QMap<QString, QStringList> titles_by_file;
  for (const Song &song : songs) {
    EXPECT_EQ(temp_dir.filePath(u"Album.cue"_s), song.cue_path());
    titles_by_file[song.url().toLocalFile()] << song.title();
  }

  ASSERT_EQ(2, titles_by_file.count());
  QStringList cd1_titles = titles_by_file.value(temp_dir.filePath(u"CD1.mp3"_s));
  cd1_titles.sort();
  EXPECT_EQ(QStringList() << u"First"_s << u"Second"_s, cd1_titles);
  EXPECT_EQ(QStringList() << u"Third"_s, titles_by_file.value(temp_dir.filePath(u"CD2.mp3"_s)));

}

// A CUE sheet which can't be read makes the CUE songs unavailable, and the media files are used as songs for the whole media files.
// When it can be read again, the CUE songs are restored, and the songs for the whole media files are not used anymore.
TEST_F(CollectionWatcherTest, UnreadableCueSheetUsesMediaFilesUntilReadable) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  int unavailable = 0;
  ASSERT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename, AvailableSongs(&unavailable));
  QList<int> cue_song_ids;
  const SongList cue_songs = backend_->GetAllSongs();
  for (const Song &song : cue_songs) cue_song_ids << song.id();
  std::sort(cue_song_ids.begin(), cue_song_ids.end());

  // Make the CUE sheet unreadable.
  const QFile::Permissions permissions = QFile::permissions(cue_filename);
  ASSERT_TRUE(QFile::setPermissions(cue_filename, QFile::Permissions()));
  {
    QFile cue_file(cue_filename);
    if (cue_file.open(QIODevice::ReadOnly)) {
      QFile::setPermissions(cue_filename, permissions);
      GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
    }
  }

  ScanDirectoryToCollection(temp_dir.path());

  // The media files are songs for the whole media files, with the title from their tags.
  const QStringList songs_while_unreadable = AvailableSongs(&unavailable);
  ASSERT_TRUE(QFile::setPermissions(cue_filename, permissions));
  ASSERT_EQ(2, songs_while_unreadable.count());
  for (const QString &song : songs_while_unreadable) {
    EXPECT_TRUE(song.endsWith(u'|')) << song.toStdString();
  }

  // Scanning again while it's unreadable doesn't change anything.
  ASSERT_TRUE(QFile::setPermissions(cue_filename, QFile::Permissions()));
  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  ASSERT_TRUE(QFile::setPermissions(cue_filename, permissions));
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

  // The CUE sheet can be read again.
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename, AvailableSongs(&unavailable));

  // The CUE songs are restored, not added again, when unavailable songs are kept.
  if (unavailable > 0) {
    QList<int> restored_ids;
    const SongList all_songs = backend_->GetAllSongs();
    for (const Song &song : all_songs) {
      if (song.has_cue() && !song.unavailable()) restored_ids << song.id();
    }
    std::sort(restored_ids.begin(), restored_ids.end());
    EXPECT_EQ(cue_song_ids, restored_ids);
  }

  // Scanning again doesn't change anything.
  const ScanResult final_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, final_scan.new_or_updated);
  EXPECT_EQ(0, final_scan.deleted);
  EXPECT_EQ(0, final_scan.unavailable);
  EXPECT_EQ(0, final_scan.touched);
  EXPECT_EQ(0, final_scan.readded);

}

// Regression test: when the CUE sheet was deleted after it was unreadable and readable again, the CUE song became the song for the whole media file, which conflicted with the unavailable song for the whole media file.
TEST_F(CollectionWatcherTest, DeletedCueSheetAfterUnreadableReusesWholeFileSong) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());
  MakeReadable(cue_filename);
  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(3, AvailableSongs().count());

  ASSERT_TRUE(QFile::remove(cue_filename));
  ScanDirectoryToCollection(temp_dir.path());

  const QStringList songs = AvailableSongs();
  ASSERT_EQ(2, songs.count());
  for (const QString &song : songs) {
    EXPECT_TRUE(song.endsWith(u'|')) << song.toStdString();
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// When the CUE sheet of the songs can't be read, but another CUE sheet has songs for the media file, that's used instead of the song for the whole media file.
TEST_F(CollectionWatcherTest, UnreadableCueSheetWithReplacementUsesReplacement) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  const QString replacement_cue_filename = temp_dir.filePath(u"Replacement.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());

  QByteArray cue;
  cue += "FILE \"CD1.mp3\" MP3\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"Replacement\"\n";
  cue += "    INDEX 01 00:00:00\n";
  WriteFile(replacement_cue_filename, cue);
  if (HasFatalFailure()) return;
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";

  ScanDirectoryToCollection(temp_dir.path());
  const QStringList songs = AvailableSongs();
  MakeReadable(cue_filename);

  // CD1 uses the replacement CUE sheet, CD2 uses the song for the whole media file.
  ASSERT_EQ(2, songs.count());
  EXPECT_TRUE(songs.contains(u"Replacement|"_s + replacement_cue_filename)) << songs.join(u", "_s).toStdString();

}

// Regression test: a CUE sheet without tracks for a media file used to claim it, so the tracks of another CUE sheet for the media file were not used.
TEST_F(CollectionWatcherTest, CueSheetWithoutTracksDoesNotClaimMediaFile) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  // Sorted before Album.cue.
  WriteFile(temp_dir.filePath(u"0-empty.cue"_s), "FILE \"CD1.mp3\" MP3\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename, AvailableSongs());

}

// Regression test: while the CUE sheet can't be read, an added LRC file for the song for the whole media file used to be ignored.
TEST_F(CollectionWatcherTest, UnreadableCueSheetDetectsAddedLrcFile) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());

  WriteFile(temp_dir.filePath(u"CD1.lrc"_s), "[00:00.00]Lyrics\n");
  if (HasFatalFailure()) return;
  ScanDirectoryToCollection(temp_dir.path());
  MakeReadable(cue_filename);

  bool found = false;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    if (!song.unavailable() && !song.has_cue() && song.url().toLocalFile().endsWith(u"/CD1.mp3"_s)) {
      found = true;
      EXPECT_NE(0, song.lrc_mtime());
    }
  }
  EXPECT_TRUE(found);

}

// Regression test: after a CUE sheet newer than the media file lost its tracks, the song for the whole media file was considered changed on every scan, since the mtime of the CUE sheet was still used.
TEST_F(CollectionWatcherTest, CueSheetWithoutTracksAnymoreIsNotRescanned) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(3, AvailableSongs().count());

  WriteFile(cue_filename, "TITLE \"Album\"\n");
  if (HasFatalFailure()) return;
  SetModificationTime(cue_filename, QDateTime::currentDateTime().addSecs(3600));
  if (HasFatalFailure()) return;
  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(2, AvailableSongs().count());

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: when the media files were restored without the CUE sheet, the unavailable CUE songs were restored instead of using the songs for the whole media files.
TEST_F(CollectionWatcherTest, MediaFilesRestoredWithoutCueSheetUseWholeFileSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  QTemporaryDir moved_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(moved_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  // The CUE songs have the mtime of the media files then, so the restored media files are not considered changed.
  SetModificationTime(cue_filename, QFileInfo(temp_dir.filePath(u"CD1.mp3"_s)).lastModified().addSecs(-3600));
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(3, AvailableSongs().count());

  const QStringList filenames = QStringList() << u"CD1.mp3"_s << u"CD2.mp3"_s << u"Album.cue"_s;
  for (const QString &filename : filenames) {
    ASSERT_TRUE(QFile::rename(temp_dir.filePath(filename), moved_dir.filePath(filename)));
  }
  ScanDirectoryToCollection(temp_dir.path());
  int unavailable = 0;
  ASSERT_EQ(0, AvailableSongs(&unavailable).count());
  if (unavailable == 0) GTEST_SKIP() << "Unavailable songs are deleted";

  // Restore the media files without the CUE sheet.
  ASSERT_TRUE(QFile::rename(moved_dir.filePath(u"CD1.mp3"_s), temp_dir.filePath(u"CD1.mp3"_s)));
  ASSERT_TRUE(QFile::rename(moved_dir.filePath(u"CD2.mp3"_s), temp_dir.filePath(u"CD2.mp3"_s)));
  ScanDirectoryToCollection(temp_dir.path());

  const QStringList songs = AvailableSongs();
  ASSERT_EQ(2, songs.count());
  for (const QString &song : songs) {
    EXPECT_TRUE(song.endsWith(u'|')) << song.toStdString();
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: when the song for the whole media file, used while the CUE sheet can't be read, could no longer be read, it was kept available.
TEST_F(CollectionWatcherTest, UnreadableCueSheetRetiresUnreadableWholeFileSong) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  const QString cd1_filename = temp_dir.filePath(u"CD1.mp3"_s);

  ScanDirectoryToCollection(temp_dir.path());
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(2, AvailableSongs().count());

  // Change the media file so it's read again, and make it unreadable.
  SetModificationTime(cd1_filename, QDateTime::currentDateTime().addSecs(3600));
  if (HasFatalFailure()) return;
  ASSERT_TRUE(MakeUnreadable(cd1_filename));
  ScanDirectoryToCollection(temp_dir.path());
  MakeReadable(cd1_filename);
  MakeReadable(cue_filename);

  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    if (song.url().toLocalFile() == cd1_filename) {
      EXPECT_TRUE(song.unavailable()) << song.title().toStdString() << " " << song.cue_path().toStdString();
    }
  }
  EXPECT_EQ(1, AvailableSongs().count());

}

// Regression test: after switching to another CUE sheet while the CUE sheet couldn't be read, and back when it could be read again, only some of the songs of the CUE sheet were restored, together with the song of the other CUE sheet.
TEST_F(CollectionWatcherTest, CueSheetReadableAgainAfterReplacementRestoresAllSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  const QString replacement_cue_filename = temp_dir.filePath(u"Replacement.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  const QStringList album_songs = QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename;
  ASSERT_EQ(album_songs, AvailableSongs());
  QList<int> album_song_ids;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) album_song_ids << song.id();
  std::sort(album_song_ids.begin(), album_song_ids.end());

  // Album.cue can't be read, CD1 uses Replacement.cue.
  QByteArray cue;
  cue += "FILE \"CD1.mp3\" MP3\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"Replacement\"\n";
  cue += "    INDEX 01 00:00:00\n";
  WriteFile(replacement_cue_filename, cue);
  if (HasFatalFailure()) return;
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());
  MakeReadable(cue_filename);
  int unavailable = 0;
  const QStringList songs_while_unreadable = AvailableSongs(&unavailable);
  ASSERT_TRUE(songs_while_unreadable.contains(u"Replacement|"_s + replacement_cue_filename)) << songs_while_unreadable.join(u", "_s).toStdString();

  // Album.cue can be read again, and is used before Replacement.cue since it's sorted first.
  ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(album_songs, AvailableSongs(&unavailable));

  // The songs keep their IDs, when unavailable songs are kept.
  if (unavailable > 0) {
    QList<int> restored_ids;
    const SongList all_songs = backend_->GetAllSongs();
    for (const Song &song : all_songs) {
      if (!song.unavailable()) restored_ids << song.id();
    }
    std::sort(restored_ids.begin(), restored_ids.end());
    EXPECT_EQ(album_song_ids, restored_ids);
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: a CUE sheet named after the media file with an uppercase suffix was not preferred over another CUE sheet sorted before it.
TEST_F(CollectionWatcherTest, CueSheetNamedAfterMediaFileWithUppercaseSuffixIsPreferred) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(u"CD1.mp3"_s)));
  MakeReadable(temp_dir.filePath(u"CD1.mp3"_s));
  WriteFile(temp_dir.filePath(u"0-album.cue"_s), "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Other\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  const QString named_cue_filename = temp_dir.filePath(u"CD1.CUE"_s);
  WriteFile(named_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Named\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"Named|"_s + named_cue_filename, AvailableSongs());

}

// Regression test: when the media files were restored with a renamed CUE sheet, the unavailable CUE songs were not reused, so the songs were added again without their statistics.
TEST_F(CollectionWatcherTest, MediaFilesRestoredWithRenamedCueSheetReuseSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  QTemporaryDir moved_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(moved_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(3, AvailableSongs().count());
  QList<int> song_ids;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) song_ids << song.id();
  std::sort(song_ids.begin(), song_ids.end());

  const QStringList filenames = QStringList() << u"CD1.mp3"_s << u"CD2.mp3"_s << u"Album.cue"_s;
  for (const QString &filename : filenames) {
    ASSERT_TRUE(QFile::rename(temp_dir.filePath(filename), moved_dir.filePath(filename)));
  }
  ScanDirectoryToCollection(temp_dir.path());
  int unavailable = 0;
  ASSERT_EQ(0, AvailableSongs(&unavailable).count());
  if (unavailable == 0) GTEST_SKIP() << "Unavailable songs are deleted";

  // Restore the media files with the CUE sheet renamed.
  ASSERT_TRUE(QFile::rename(moved_dir.filePath(u"CD1.mp3"_s), temp_dir.filePath(u"CD1.mp3"_s)));
  ASSERT_TRUE(QFile::rename(moved_dir.filePath(u"CD2.mp3"_s), temp_dir.filePath(u"CD2.mp3"_s)));
  const QString renamed_cue_filename = temp_dir.filePath(u"Renamed.cue"_s);
  ASSERT_TRUE(QFile::rename(moved_dir.filePath(u"Album.cue"_s), renamed_cue_filename));
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + renamed_cue_filename << u"Second|"_s + renamed_cue_filename << u"Third|"_s + renamed_cue_filename, AvailableSongs(&unavailable));
  EXPECT_EQ(0, unavailable);
  QList<int> restored_ids;
  const SongList restored_songs = backend_->GetAllSongs();
  for (const Song &song : restored_songs) restored_ids << song.id();
  std::sort(restored_ids.begin(), restored_ids.end());
  EXPECT_EQ(song_ids, restored_ids);

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: a CUE sheet named after the media file with a different Unicode normalization form was not preferred over another CUE sheet sorted before it.
TEST_F(CollectionWatcherTest, CueSheetNamedAfterMediaFileWithOtherNormalizationIsPreferred) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString media_name = u"Caf\u00e9.mp3"_s;
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(media_name)));
  MakeReadable(temp_dir.filePath(media_name));
  const QByteArray file_line = "FILE \"" + media_name.toUtf8() + "\" MP3\n";
  WriteFile(temp_dir.filePath(u"0-album.cue"_s), file_line + "  TRACK 01 AUDIO\n    TITLE \"Other\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  // The CUE sheet is named in the decomposed form.
  const QString named_cue_filename = temp_dir.filePath(QString(media_name + u".cue"_s).normalized(QString::NormalizationForm_D));
  WriteFile(named_cue_filename, file_line + "  TRACK 01 AUDIO\n    TITLE \"Named\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  if (QDir(temp_dir.path()).entryList(QStringList() << u"*.cue"_s, QDir::Files).count() != 2) GTEST_SKIP() << "The filesystem normalizes the filenames";
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  const QStringList songs = AvailableSongs();
  ASSERT_EQ(1, songs.count());
  EXPECT_TRUE(songs.first().startsWith(u"Named|"_s)) << songs.first().toStdString();

}

// Regression test: when the media files were restored unchanged after a replacement CUE sheet was used for some of the songs, only the songs kept for the CUE sheet were restored, not all songs of the CUE sheet.
TEST_F(CollectionWatcherTest, MediaFilesRestoredAfterReplacementRestoreAllCueSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  QTemporaryDir moved_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(moved_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  const QString replacement_cue_filename = temp_dir.filePath(u"Replacement.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  const QStringList album_songs = QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename;
  ASSERT_EQ(album_songs, AvailableSongs());
  QList<int> song_ids;
  int first_song_id = -1;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    song_ids << song.id();
    if (song.title() == u"First"_s) first_song_id = song.id();
  }
  std::sort(song_ids.begin(), song_ids.end());
  ASSERT_NE(-1, first_song_id);
  backend_->IncrementPlayCount(first_song_id);

  // Album.cue can't be read, CD1 uses Replacement.cue, which reuses the first song of Album.cue.
  WriteFile(replacement_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Replacement\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());

  // The media files are removed.
  const QStringList filenames = QStringList() << u"CD1.mp3"_s << u"CD2.mp3"_s;
  for (const QString &filename : filenames) {
    ASSERT_TRUE(QFile::rename(temp_dir.filePath(filename), moved_dir.filePath(filename)));
  }
  ScanDirectoryToCollection(temp_dir.path());
  MakeReadable(cue_filename);
  int unavailable = 0;
  ASSERT_EQ(0, AvailableSongs(&unavailable).count());
  if (unavailable == 0) GTEST_SKIP() << "Unavailable songs are deleted";

  // The media files are restored unchanged, Album.cue can be read again, and Replacement.cue is removed.
  for (const QString &filename : filenames) {
    ASSERT_TRUE(QFile::rename(moved_dir.filePath(filename), temp_dir.filePath(filename)));
  }
  ASSERT_TRUE(QFile::remove(replacement_cue_filename));
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(album_songs, AvailableSongs());

  // The songs keep their IDs and statistics.
  QList<int> restored_ids;
  const SongList restored_songs = backend_->GetAllSongs();
  for (const Song &song : restored_songs) {
    if (song.unavailable()) continue;
    restored_ids << song.id();
    if (song.title() == u"First"_s) {
      EXPECT_EQ(first_song_id, song.id());
      EXPECT_EQ(1, song.playcount());
    }
  }
  std::sort(restored_ids.begin(), restored_ids.end());
  EXPECT_EQ(song_ids, restored_ids);

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: when the CUE sheet was renamed to another Unicode normalization form, the old path was kept, since the normalized paths were compared.
TEST_F(CollectionWatcherTest, CueSheetRenamedToOtherNormalizationStoresNewPath) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  const QString composed_cue_filename = temp_dir.filePath(u"Caf\u00e9.cue"_s);
  const QString decomposed_cue_filename = temp_dir.filePath(QString(u"Caf\u00e9.cue"_s).normalized(QString::NormalizationForm_D));
  ASSERT_TRUE(QFile::rename(temp_dir.filePath(u"Album.cue"_s), composed_cue_filename));
  // The media files are newer than the CUE sheet, so the songs have the mtime of the media files.
  SetModificationTime(composed_cue_filename, QFileInfo(temp_dir.filePath(u"CD1.mp3"_s)).lastModified().addSecs(-3600));
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(QStringList() << u"First|"_s + composed_cue_filename << u"Second|"_s + composed_cue_filename << u"Third|"_s + composed_cue_filename, AvailableSongs());

  // On filesystems which don't distinguish Unicode normalization forms, the decomposed name refers to the same file.
  if (QFile::exists(decomposed_cue_filename)) GTEST_SKIP() << "The filesystem normalizes the filenames";
  ASSERT_TRUE(QFile::rename(composed_cue_filename, decomposed_cue_filename));
  if (QFile::exists(composed_cue_filename)) GTEST_SKIP() << "The filesystem normalizes the filenames";
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + decomposed_cue_filename << u"Second|"_s + decomposed_cue_filename << u"Third|"_s + decomposed_cue_filename, AvailableSongs());

}

// Regression test: on case-insensitive filesystems, a CUE sheet named after the media file in another case was not preferred over another CUE sheet sorted before it.
// On case-sensitive filesystems the names are different, so the CUE sheet sorted first is used.
TEST_F(CollectionWatcherTest, CueSheetNamedAfterMediaFileInOtherCase) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(u"CD1.mp3"_s)));
  MakeReadable(temp_dir.filePath(u"CD1.mp3"_s));
  const QString other_cue_filename = temp_dir.filePath(u"0-album.cue"_s);
  WriteFile(other_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Other\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  const QString named_cue_filename = temp_dir.filePath(u"cd1.cue"_s);
  WriteFile(named_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Named\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  const bool case_insensitive = QFile::exists(temp_dir.filePath(u"CD1.cue"_s));
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  const QStringList songs = AvailableSongs();
  ASSERT_EQ(1, songs.count());
  if (case_insensitive) {
    EXPECT_TRUE(songs.first().startsWith(u"Named|"_s)) << songs.first().toStdString();
  }
  else {
    EXPECT_EQ(u"Other|"_s + other_cue_filename, songs.first());
  }

}

// Regression test: on case-insensitive filesystems, a CUE sheet with a FILE line in another case than the media file was not associated with it.
// On case-sensitive filesystems, the FILE line refers to another file, which doesn't exist.
TEST_F(CollectionWatcherTest, CueSheetFileLineInOtherCase) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString media_filename = temp_dir.filePath(u"CD1.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, media_filename));
  MakeReadable(media_filename);
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  WriteFile(cue_filename, "FILE \"cd1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"First\"\n    INDEX 01 00:00:00\n  TRACK 02 AUDIO\n    TITLE \"Second\"\n    INDEX 01 00:01:00\n");
  if (HasFatalFailure()) return;
  const bool case_insensitive = QFile::exists(temp_dir.filePath(u"cd1.mp3"_s));
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  const QStringList songs = AvailableSongs();
  if (case_insensitive) {
    EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename, songs);
    // The songs have the name of the media file on disk.
    const SongList all_songs = backend_->GetAllSongs();
    for (const Song &song : all_songs) {
      EXPECT_EQ(QUrl::fromLocalFile(media_filename), song.url());
    }
    const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
    EXPECT_EQ(0, unchanged_scan.new_or_updated);
    EXPECT_EQ(0, unchanged_scan.deleted);
    EXPECT_EQ(0, unchanged_scan.unavailable);
    EXPECT_EQ(0, unchanged_scan.touched);
    EXPECT_EQ(0, unchanged_scan.readded);
  }
  else {
    ASSERT_EQ(1, songs.count());
    EXPECT_TRUE(songs.first().endsWith(u'|')) << songs.first().toStdString();
  }

}

// Regression test: when the CUE sheet was renamed while it couldn't be read, only the song for the whole media file was reused, so the other CUE songs were added again without their statistics.
TEST_F(CollectionWatcherTest, CueSheetRenamedWhileUnreadableReusesSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  const QString renamed_cue_filename = temp_dir.filePath(u"Renamed.cue"_s);

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(3, AvailableSongs().count());
  QList<int> song_ids;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) song_ids << song.id();
  std::sort(song_ids.begin(), song_ids.end());

  if (!MakeUnreadable(cue_filename)) GTEST_SKIP() << "The CUE sheet can still be read, the test is probably running as root";
  ScanDirectoryToCollection(temp_dir.path());
  int unavailable = 0;
  const QStringList songs_while_unreadable = AvailableSongs(&unavailable);
  if (unavailable == 0) {
    MakeReadable(cue_filename);
    GTEST_SKIP() << "Unavailable songs are deleted";
  }
  ASSERT_EQ(2, songs_while_unreadable.count());

  ASSERT_TRUE(QFile::rename(cue_filename, renamed_cue_filename));
  MakeReadable(renamed_cue_filename);
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + renamed_cue_filename << u"Second|"_s + renamed_cue_filename << u"Third|"_s + renamed_cue_filename, AvailableSongs());
  QList<int> restored_ids;
  const SongList all_songs = backend_->GetAllSongs();
  for (const Song &song : all_songs) {
    if (!song.unavailable()) restored_ids << song.id();
  }
  std::sort(restored_ids.begin(), restored_ids.end());
  EXPECT_EQ(song_ids, restored_ids);

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: two CUE sheets whose names only differ in Unicode normalization form were treated as the same CUE sheet, so the songs of one were used for the media file of the other.
TEST_F(CollectionWatcherTest, CueSheetsWithNamesInDifferentNormalizationFormsAreDifferent) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  for (const QString &filename : QStringList{u"CD1.mp3"_s, u"CD2.mp3"_s}) {
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(filename)));
    MakeReadable(temp_dir.filePath(filename));
  }
  const QString composed_cue_filename = temp_dir.filePath(u"Caf\u00e9.cue"_s);
  const QString decomposed_cue_filename = temp_dir.filePath(QString(u"Caf\u00e9.cue"_s).normalized(QString::NormalizationForm_D));
  WriteFile(composed_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Composed\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  if (QFile::exists(decomposed_cue_filename)) GTEST_SKIP() << "The filesystem normalizes the filenames";
  WriteFile(decomposed_cue_filename, "FILE \"CD2.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Decomposed\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  QStringList expected_songs = QStringList() << u"Composed|"_s + composed_cue_filename << u"Decomposed|"_s + decomposed_cue_filename;
  expected_songs.sort();
  EXPECT_EQ(expected_songs, AvailableSongs());

}

// Regression test: two media files whose names only differ in Unicode normalization form were treated as the same media file, so the CUE songs of one were also added for the other.
TEST_F(CollectionWatcherTest, MediaFilesWithNamesInDifferentNormalizationFormsAreDifferent) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString composed_media_name = u"Caf\u00e9.mp3"_s;
  const QString decomposed_media_name = composed_media_name.normalized(QString::NormalizationForm_D);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(composed_media_name)));
  MakeReadable(temp_dir.filePath(composed_media_name));
  if (QFile::exists(temp_dir.filePath(decomposed_media_name))) GTEST_SKIP() << "The filesystem normalizes the filenames";
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(decomposed_media_name)));
  MakeReadable(temp_dir.filePath(decomposed_media_name));
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  WriteFile(cue_filename, "FILE \"" + composed_media_name.toUtf8() + "\" MP3\n  TRACK 01 AUDIO\n    TITLE \"First\"\n    INDEX 01 00:00:00\n  TRACK 02 AUDIO\n    TITLE \"Second\"\n    INDEX 01 00:01:00\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  int cue_songs = 0;
  int file_songs = 0;
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    if (song.has_cue()) {
      ++cue_songs;
      EXPECT_EQ(QUrl::fromLocalFile(temp_dir.filePath(composed_media_name)), song.url());
    }
    else {
      ++file_songs;
      EXPECT_EQ(QUrl::fromLocalFile(temp_dir.filePath(decomposed_media_name)), song.url());
    }
  }
  EXPECT_EQ(2, cue_songs);
  EXPECT_EQ(1, file_songs);

  // Scanning again doesn't mix the songs of the two media files.
  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);
  EXPECT_EQ(3, AvailableSongs().count());

}

// Regression test: on case-sensitive filesystems, another CUE sheet named after the media file in another case made a CUE sheet in another case preferred.
TEST_F(CollectionWatcherTest, CueSheetInOtherCaseIsNotPreferredBecauseOfAnotherFile) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, temp_dir.filePath(u"CD1.mp3"_s)));
  MakeReadable(temp_dir.filePath(u"CD1.mp3"_s));
  const QString other_cue_filename = temp_dir.filePath(u"0-album.cue"_s);
  WriteFile(other_cue_filename, "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Other\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  WriteFile(temp_dir.filePath(u"cd1.cue"_s), "FILE \"CD1.mp3\" MP3\n  TRACK 01 AUDIO\n    TITLE \"Lowercase\"\n    INDEX 01 00:00:00\n");
  if (HasFatalFailure()) return;
  if (QFile::exists(temp_dir.filePath(u"CD1.cue"_s))) GTEST_SKIP() << "The filesystem is case-insensitive";
  // A separate CUE sheet named after the media file, without tracks.
  WriteFile(temp_dir.filePath(u"CD1.cue"_s), "TITLE \"Empty\"\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"Other|"_s + other_cue_filename, AvailableSongs());

}

// When a CUE sheet is added, the CUE songs are added as new songs, the song for the whole media file is not used for them, and is marked unavailable or deleted.
TEST_F(CollectionWatcherTest, CueSheetAddedAddsCueSongsAsNewSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  QFile cue_file(cue_filename);
  ASSERT_TRUE(cue_file.open(QIODevice::ReadOnly));
  const QByteArray cue = cue_file.readAll();
  cue_file.close();
  ASSERT_TRUE(QFile::remove(cue_filename));
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  QSet<int> file_song_ids;
  const SongList file_songs = backend_->GetAllSongs();
  ASSERT_EQ(2, file_songs.count());
  for (const Song &song : file_songs) {
    ASSERT_FALSE(song.has_cue());
    file_song_ids.insert(song.id());
    backend_->IncrementPlayCount(song.id());
  }

  // An album cover in the directory, added before the CUE sheet, so the songs for the whole media files are not rescanned for it first.
  const QString cover_filename = temp_dir.filePath(u"cover.png"_s);
  QImage cover(16, 16, QImage::Format_RGB32);
  cover.fill(Qt::red);
  ASSERT_TRUE(cover.save(cover_filename, "PNG"));

  WriteFile(cue_filename, cue);
  if (HasFatalFailure()) return;
  ScanDirectoryToCollection(temp_dir.path());

  EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename, AvailableSongs());
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    if (song.has_cue()) {
      EXPECT_FALSE(file_song_ids.contains(song.id())) << song.title().toStdString();
      EXPECT_EQ(0, song.playcount()) << song.title().toStdString();
      // The new CUE songs have the album cover of the directory.
      EXPECT_EQ(QUrl::fromLocalFile(cover_filename), song.art_automatic()) << song.title().toStdString();
      EXPECT_NE(0, song.art_automatic_mtime()) << song.title().toStdString();
    }
    else {
      EXPECT_TRUE(song.unavailable());
      EXPECT_EQ(1, song.playcount());
    }
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// When a CUE sheet is deleted, the media file is added as a new song, the CUE songs are not used for it, and are marked unavailable or deleted.
TEST_F(CollectionWatcherTest, CueSheetDeletedAddsMediaFileAsNewSong) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  CreateAlbumWithCueSheet(temp_dir);
  if (HasFatalFailure()) return;
  const QString cue_filename = temp_dir.filePath(u"Album.cue"_s);
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  QSet<int> cue_song_ids;
  const SongList cue_songs = backend_->GetAllSongs();
  ASSERT_EQ(3, cue_songs.count());
  for (const Song &song : cue_songs) {
    ASSERT_TRUE(song.has_cue());
    cue_song_ids.insert(song.id());
    backend_->IncrementPlayCount(song.id());
  }

  ASSERT_TRUE(QFile::remove(cue_filename));
  ScanDirectoryToCollection(temp_dir.path());

  const QStringList available_songs = AvailableSongs();
  ASSERT_EQ(2, available_songs.count());
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    if (song.has_cue()) {
      EXPECT_TRUE(song.unavailable()) << song.title().toStdString();
      EXPECT_EQ(1, song.playcount()) << song.title().toStdString();
    }
    else {
      EXPECT_FALSE(song.unavailable());
      EXPECT_FALSE(cue_song_ids.contains(song.id()));
      EXPECT_EQ(0, song.playcount());
    }
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: on filesystems which don't distinguish the case or Unicode normalization form, an absolute FILE line with a directory in another spelling was not associated with the media file.
TEST_F(CollectionWatcherTest, CueSheetAbsoluteFileLineWithDirectoryInOtherSpelling) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString directory_name = u"Caf\u00e9"_s;
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(directory_name));
  const QString album_path = temp_dir.filePath(directory_name);
  // The directory in the FILE line in decomposed form and uppercase.
  const QString other_album_path = temp_dir.filePath(directory_name.normalized(QString::NormalizationForm_D).toUpper());
  if (!QFileInfo::exists(other_album_path)) GTEST_SKIP() << "The filesystem distinguishes the case and Unicode normalization form";

  const QString media_filename = album_path + u"/CD1.mp3"_s;
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, media_filename));
  MakeReadable(media_filename);
  const QString cue_filename = album_path + u"/Album.cue"_s;
  WriteFile(cue_filename, "FILE \"" + (other_album_path + u"/CD1.mp3"_s).toUtf8() + "\" MP3\n  TRACK 01 AUDIO\n    TITLE \"First\"\n    INDEX 01 00:00:00\n  TRACK 02 AUDIO\n    TITLE \"Second\"\n    INDEX 01 00:01:00\n");
  if (HasFatalFailure()) return;
  backend_->AddDirectory(album_path);

  ScanDirectoryToCollection(album_path);

  EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename, AvailableSongs());
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    EXPECT_EQ(QUrl::fromLocalFile(media_filename), song.url());
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(album_path);
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: after deleting one of two media files whose names only differ in Unicode normalization form, its songs were used for the other media file, and were never marked unavailable or deleted.
TEST_F(CollectionWatcherTest, DeletedMediaFileWithNameInOtherNormalizationFormIsDeleted) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString composed_filename = temp_dir.filePath(u"Caf\u00e9.mp3"_s);
  const QString decomposed_filename = temp_dir.filePath(QString(u"Caf\u00e9.mp3"_s).normalized(QString::NormalizationForm_D));
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, composed_filename));
  MakeReadable(composed_filename);
  if (QFile::exists(decomposed_filename)) GTEST_SKIP() << "The filesystem normalizes the filenames";
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, decomposed_filename));
  MakeReadable(decomposed_filename);
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(2, AvailableSongs().count());

  ASSERT_TRUE(QFile::remove(decomposed_filename));
  ScanDirectoryToCollection(temp_dir.path());

  const SongList songs = backend_->GetAllSongs();
  int available = 0;
  for (const Song &song : songs) {
    if (song.unavailable()) {
      EXPECT_EQ(QUrl::fromLocalFile(decomposed_filename), song.url());
    }
    else {
      ++available;
      EXPECT_EQ(QUrl::fromLocalFile(composed_filename), song.url());
    }
  }
  EXPECT_EQ(1, available);

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: on case-insensitive filesystems, a collection directory added with another case than on disk lost its CUE songs, since the media files of the CUE sheets used the spelling on disk.
TEST_F(CollectionWatcherTest, CollectionDirectoryInOtherCaseUsesCueSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"Album"_s));
  const QString other_case_path = temp_dir.filePath(u"album"_s);
  if (!QFileInfo::exists(other_case_path)) GTEST_SKIP() << "The filesystem is case-sensitive";

  QTemporaryDir album_dir(temp_dir.filePath(u"Album/XXXXXX"_s));
  ASSERT_TRUE(album_dir.isValid());
  CreateAlbumWithCueSheet(album_dir);
  if (HasFatalFailure()) return;
  // The collection directory with the other case.
  const QString collection_path = other_case_path + u'/' + QFileInfo(album_dir.path()).fileName();
  backend_->AddDirectory(collection_path);

  ScanDirectoryToCollection(collection_path);

  const QString cue_filename = collection_path + u"/Album.cue"_s;
  EXPECT_EQ(QStringList() << u"First|"_s + cue_filename << u"Second|"_s + cue_filename << u"Third|"_s + cue_filename, AvailableSongs());
  // The songs have the spelling of the collection directory, like the media files scanned.
  const SongList songs = backend_->GetAllSongs();
  for (const Song &song : songs) {
    EXPECT_TRUE(song.url().toLocalFile().startsWith(collection_path + u'/')) << song.url().toLocalFile().toStdString();
  }

  const ScanResult unchanged_scan = ScanDirectoryToCollection(collection_path);
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);

}

// Regression test: the songs of two directories whose names only differ in Unicode normalization form were mixed, so the song of the other directory was marked unavailable or deleted.
TEST_F(CollectionWatcherTest, DirectoriesWithNamesInDifferentNormalizationFormsAreDifferent) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString composed_name = u"Caf\u00e9"_s;
  const QString decomposed_name = composed_name.normalized(QString::NormalizationForm_D);
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(composed_name));
  if (QFileInfo::exists(temp_dir.filePath(decomposed_name))) GTEST_SKIP() << "The filesystem normalizes the filenames";
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(decomposed_name));
  for (const QString &directory_name : QStringList{composed_name, decomposed_name}) {
    const QString filename = temp_dir.filePath(directory_name + u"/track.mp3"_s);
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    MakeReadable(filename);
  }
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path());
  ASSERT_EQ(2, AvailableSongs().count());

  const ScanResult unchanged_scan = ScanDirectoryToCollection(temp_dir.path());
  EXPECT_EQ(0, unchanged_scan.new_or_updated);
  EXPECT_EQ(0, unchanged_scan.deleted);
  EXPECT_EQ(0, unchanged_scan.unavailable);
  EXPECT_EQ(0, unchanged_scan.touched);
  EXPECT_EQ(0, unchanged_scan.readded);
  EXPECT_EQ(2, AvailableSongs().count());

}

// Regression test: on filesystems which don't distinguish the Unicode normalization form, the songs of a directory scanned with a parent directory in another normalization form were not used, so they were added again.
TEST_F(CollectionWatcherTest, DirectoryWithParentInOtherNormalizationFormKeepsSongs) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  const QString composed_name = u"Caf\u00e9"_s;
  const QString decomposed_name = composed_name.normalized(QString::NormalizationForm_D);
  ASSERT_TRUE(QDir(temp_dir.path()).mkpath(composed_name + u"/Album"_s));
  const QString composed_path = temp_dir.filePath(composed_name + u"/Album"_s);
  const QString decomposed_path = temp_dir.filePath(decomposed_name + u"/Album"_s);
  if (!QFileInfo::exists(decomposed_path)) GTEST_SKIP() << "The filesystem distinguishes the Unicode normalization form";
  const QString filename = composed_path + u"/track.mp3"_s;
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  MakeReadable(filename);
  backend_->AddDirectory(composed_path);

  ScanDirectoryToCollection(composed_path);
  const SongList songs = backend_->GetAllSongs();
  ASSERT_EQ(1, songs.count());
  const int song_id = songs.first().id();

  // The same directory, with the parent directory in the decomposed form.
  const ScanResult scan = ScanDirectoryToCollection(decomposed_path);
  EXPECT_EQ(0, scan.new_or_updated);
  EXPECT_EQ(0, scan.deleted);
  EXPECT_EQ(0, scan.unavailable);

  const SongList songs_after_scan = backend_->GetAllSongs();
  ASSERT_EQ(1, songs_after_scan.count());
  EXPECT_EQ(song_id, songs_after_scan.first().id());
  EXPECT_FALSE(songs_after_scan.first().unavailable());

}

// A media file moved to another directory in the collection is found by fingerprint, and keeps its song, also when the old directory is scanned in the same transaction.
TEST_F(CollectionWatcherTest, MediaFileMovedToAnotherDirectoryIsFoundByFingerprint) {

  QTemporaryDir temp_dir(QDir::current().filePath(u"collectionwatcher_test-XXXXXX"_s));
  ASSERT_TRUE(temp_dir.isValid());
  // One media file is moved to a directory scanned after the old directory, and one to a directory scanned before the old directory.
  for (const QStringList &directory_names : QList<QStringList>{{u"A"_s, u"B"_s}, {u"D"_s, u"C"_s}}) {
    const QString old_directory = temp_dir.filePath(directory_names.at(0));
    const QString new_directory = temp_dir.filePath(directory_names.at(1));
    ASSERT_TRUE(QDir().mkpath(old_directory));
    ASSERT_TRUE(QDir().mkpath(new_directory));
  }
  const QString old_filename = temp_dir.filePath(u"A/track.wav"_s);
  const QString old_filename2 = temp_dir.filePath(u"D/track2.wav"_s);
  WriteWavFile(old_filename, 1);
  if (HasFatalFailure()) return;
  WriteWavFile(old_filename2, 2);
  if (HasFatalFailure()) return;
  backend_->AddDirectory(temp_dir.path());

  ScanDirectoryToCollection(temp_dir.path(), true);
  const SongList songs = backend_->GetAllSongs();
  ASSERT_EQ(2, songs.count());
  for (const Song &song : songs) {
    if (song.fingerprint().isEmpty() || song.fingerprint() == u"NONE"_s) GTEST_SKIP() << "No fingerprint for the test media file";
  }
  // The song ID of each media file, so each moved media file is checked against its own song.
  QHash<QString, int> song_ids;
  for (const Song &song : songs) song_ids.insert(song.url().toLocalFile(), song.id());
  ASSERT_TRUE(song_ids.contains(old_filename));
  ASSERT_TRUE(song_ids.contains(old_filename2));

  // Move the media files, from A to B, which is scanned after A, and from D to C, which is scanned before D.
  // The directories are scanned in this order in one transaction, the order of the directories found when scanning the whole directory isn't defined.
  const QString new_filename = temp_dir.filePath(u"B/track.wav"_s);
  const QString new_filename2 = temp_dir.filePath(u"C/track2.wav"_s);
  ASSERT_TRUE(QFile::rename(old_filename, new_filename));
  ASSERT_TRUE(QFile::rename(old_filename2, new_filename2));
  const QStringList subdirectories = QStringList() << temp_dir.filePath(u"A"_s) << temp_dir.filePath(u"B"_s) << temp_dir.filePath(u"C"_s) << temp_dir.filePath(u"D"_s);
  const ScanResult scan = ScanDirectoryToCollection(temp_dir.path(), true, subdirectories);
  EXPECT_EQ(0, scan.deleted);
  EXPECT_EQ(0, scan.unavailable);

  QHash<QString, int> moved_song_ids;
  const SongList moved_songs = backend_->GetAllSongs();
  ASSERT_EQ(2, moved_songs.count());
  for (const Song &song : moved_songs) {
    EXPECT_FALSE(song.unavailable()) << song.url().toLocalFile().toStdString();
    moved_song_ids.insert(song.url().toLocalFile(), song.id());
  }
  ASSERT_TRUE(moved_song_ids.contains(new_filename));
  ASSERT_TRUE(moved_song_ids.contains(new_filename2));
  EXPECT_EQ(song_ids.value(old_filename), moved_song_ids.value(new_filename));
  EXPECT_EQ(song_ids.value(old_filename2), moved_song_ids.value(new_filename2));

}

}  // namespace
