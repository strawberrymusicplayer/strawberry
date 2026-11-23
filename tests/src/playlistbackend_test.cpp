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

#include <atomic>
#include <thread>

#include "gtest_include.h"

#include <QtGlobal>
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <QUuid>

#ifndef Q_OS_WIN32
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistitem.h"
#include "playlist/songplaylistitem.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistBackendTest : public ::testing::Test {
 protected:
  PlaylistBackendTest()
      : tagreader_client_(new TagReaderClient()),
        tagreader_client_thread_(new QThread()) {
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  ~PlaylistBackendTest() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  void SetUp() override {

    ASSERT_TRUE(temp_dir_.isValid());
    // The CUE sheets refer to this file, the songs from a CUE sheet are only complete when the file exists.
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.flac"_s, temp_dir_.filePath(u"album.flac"_s)));
    database_ = make_shared<Database>(make_shared<TaskManager>(), nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, make_shared<TaskManager>(), Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    playlist_backend_ = make_shared<PlaylistBackend>(database_, tagreader_client_, collection_backend_);

  }

  void TearDown() override {
    database_->Close();
  }

  // Saves a playlist with a song from a CUE sheet, with the user set data in user_set_data.
  int SaveCueSongPlaylist(const QString &cue_path, const QString &audio_path, QUuid *uuid = nullptr, const Song &user_set_data = Song()) const {

    const int playlist_id = playlist_backend_->CreatePlaylist(u"CUE"_s, QString());
    if (playlist_id == -1) return -1;

    Song song(Song::Source::LocalFile);
    song.Init(u"Saved title"_s, u"Artist"_s, u"Album"_s, 0, 180'000'000'000LL);
    song.set_url(QUrl::fromLocalFile(audio_path));
    song.set_cue_path(cue_path);
    song.set_filetype(Song::FileType::FLAC);
    song.set_valid(true);
    song.MergeUserSetData(user_set_data, false, false);
    song.set_playcount(user_set_data.playcount());
    song.set_rating(user_set_data.rating());

    const PlaylistItemPtr item = make_shared<SongPlaylistItem>(song);
    if (uuid) *uuid = item->uuid();
    PlaylistItemSaveDataList items_save_data;
    items_save_data << item->CreateSaveData();
    playlist_backend_->SavePlaylist(playlist_id, items_save_data, -1, nullptr);

    return playlist_id;

  }

  QTemporaryDir temp_dir_;
  SharedPtr<TagReaderClient> tagreader_client_;
  QThread *tagreader_client_thread_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<PlaylistBackend> playlist_backend_;
};

TEST_F(PlaylistBackendTest, RestoresCueData) {

  const QString cue_path = temp_dir_.filePath(u"album.cue"_s);
  const QString audio_path = temp_dir_.filePath(u"album.flac"_s);
  {
    QFile cue_file(cue_path);
    ASSERT_TRUE(cue_file.open(QIODevice::WriteOnly));
    cue_file.write("FILE \"album.flac\" WAVE\n  TRACK 01 AUDIO\n    TITLE \"CUE title\"\n    INDEX 01 00:00:00\n");
  }
  const int playlist_id = SaveCueSongPlaylist(cue_path, audio_path);
  ASSERT_NE(-1, playlist_id);

  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(playlist_id);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"CUE title"_s, items.first()->EffectiveMetadata().title());

  const SongList songs = playlist_backend_->GetPlaylistSongs(playlist_id);
  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(u"CUE title"_s, songs.first().title());

}

// Regression test: the restored item was replaced with a new item, which got a new ID, so the whole playlist was saved again every time it was loaded, and the play statistics, the rating and the cover set by the user were lost.
TEST_F(PlaylistBackendTest, RestoredCueItemKeepsIdAndUserSetData) {

  const QString cue_path = temp_dir_.filePath(u"album.cue"_s);
  const QString audio_path = temp_dir_.filePath(u"album.flac"_s);
  {
    QFile cue_file(cue_path);
    ASSERT_TRUE(cue_file.open(QIODevice::WriteOnly));
    cue_file.write("FILE \"album.flac\" WAVE\n  TRACK 01 AUDIO\n    TITLE \"CUE title\"\n    INDEX 01 00:00:00\n");
  }
  Song user_set_data;
  user_set_data.set_playcount(5);
  user_set_data.set_skipcount(2);
  user_set_data.set_lastplayed(1'700'000'000);
  user_set_data.set_rating(0.8F);
  user_set_data.set_art_manual(QUrl::fromLocalFile(temp_dir_.filePath(u"cover.jpg"_s)));
  QUuid uuid;
  const int playlist_id = SaveCueSongPlaylist(cue_path, audio_path, &uuid, user_set_data);
  ASSERT_NE(-1, playlist_id);
  ASSERT_FALSE(uuid.isNull());

  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(playlist_id);
  ASSERT_EQ(1, items.count());
  const PlaylistItemPtr item = items.first();
  EXPECT_EQ(uuid, item->uuid());
  EXPECT_FALSE(item->uuid_generated());

  const Song song = item->EffectiveMetadata();
  EXPECT_EQ(u"CUE title"_s, song.title());
  EXPECT_EQ(cue_path, song.cue_path());
  EXPECT_EQ(5, song.playcount());
  EXPECT_EQ(2, song.skipcount());
  EXPECT_EQ(1'700'000'000, song.lastplayed());
  EXPECT_FLOAT_EQ(0.8F, song.rating());
  EXPECT_EQ(user_set_data.art_manual(), song.art_manual());

}

// Regression test: a saved play count of 0 and a saved rating of 0 or not set were replaced with the ones of the audio file, which all the tracks of the CUE sheet share.
TEST_F(PlaylistBackendTest, RestoredCueItemKeepsZeroPlaycountAndRating) {

  const QString cue_path = temp_dir_.filePath(u"album.cue"_s);
  const QString audio_path = temp_dir_.filePath(u"album.flac"_s);
  {
    QFile cue_file(cue_path);
    ASSERT_TRUE(cue_file.open(QIODevice::WriteOnly));
    cue_file.write("FILE \"album.flac\" WAVE\n  TRACK 01 AUDIO\n    TITLE \"CUE title\"\n    INDEX 01 00:00:00\n");
  }
  // The copy of the resource file is read-only.
  ASSERT_TRUE(QFile::setPermissions(audio_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
  ASSERT_TRUE(tagreader_client_->SaveSongPlaycountBlocking(audio_path, 7).success());
  ASSERT_TRUE(tagreader_client_->SaveSongRatingBlocking(audio_path, 0.6F).success());
  Song file_song;
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(audio_path, &file_song).success());
  ASSERT_EQ(7, file_song.playcount());
  ASSERT_FLOAT_EQ(0.6F, file_song.rating());

  Song user_set_data;
  user_set_data.set_playcount(0);
  user_set_data.set_rating(0.0F);
  const int playlist_id = SaveCueSongPlaylist(cue_path, audio_path, nullptr, user_set_data);
  ASSERT_NE(-1, playlist_id);

  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(playlist_id);
  ASSERT_EQ(1, items.count());
  const Song song = items.first()->EffectiveMetadata();
  EXPECT_EQ(u"CUE title"_s, song.title());
  EXPECT_EQ(0, song.playcount());
  // A rating of 0 is saved as not set.
  EXPECT_FLOAT_EQ(-1.0F, song.rating());

}

#ifndef Q_OS_WIN32

// Regression test: the CUE sheets were read while the database mutex was held, so other threads, also the GUI thread, waited for the files to be read.
TEST_F(PlaylistBackendTest, CueDataIsRestoredWithoutDatabaseMutex) {

  // The CUE sheet is a FIFO, opening it for reading waits until it's opened for writing, so the test can check the mutex while the CUE sheet is read.
  const QString cue_path = temp_dir_.filePath(u"album.cue"_s);
  const QString audio_path = temp_dir_.filePath(u"album.flac"_s);
  const QByteArray cue_path_data = QFile::encodeName(cue_path);
  ASSERT_EQ(0, mkfifo(cue_path_data.constData(), 0600));
  const int playlist_id = SaveCueSongPlaylist(cue_path, audio_path);
  ASSERT_NE(-1, playlist_id);

  std::atomic<bool> items_returned = false;
  std::atomic<bool> mutex_locked = false;
  std::thread cue_writer([this, &cue_path_data, &items_returned, &mutex_locked]() {
    // Opening for writing returns when the playlist backend has opened the CUE sheet for reading.
    const int fd = open(cue_path_data.constData(), O_WRONLY);
    if (fd < 0) return;
    // The test opened the CUE sheet for reading, since the playlist backend didn't.
    if (items_returned) {
      close(fd);
      return;
    }
    if (database_->Mutex()->tryLock(2000)) {
      mutex_locked = true;
      database_->Mutex()->unlock();
    }
    const QByteArray cue_data = "FILE \"album.flac\" WAVE\n  TRACK 01 AUDIO\n    TITLE \"CUE title\"\n    INDEX 01 00:00:00\n";
    const ssize_t written = write(fd, cue_data.constData(), static_cast<size_t>(cue_data.size()));
    Q_UNUSED(written);
    close(fd);
  });

  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(playlist_id);

  // When the playlist backend didn't open the CUE sheet, the writer waits for a reader, so it's opened for reading here, and the test fails instead of hanging.
  items_returned = true;
  const int fd = open(cue_path_data.constData(), O_RDONLY | O_NONBLOCK);
  cue_writer.join();
  if (fd >= 0) close(fd);

  EXPECT_TRUE(mutex_locked);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"CUE title"_s, items.first()->EffectiveMetadata().title());

}

#endif  // Q_OS_WIN32

}  // namespace
