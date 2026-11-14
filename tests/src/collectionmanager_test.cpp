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

#include "gtest_include.h"

#include <functional>

#include <QObject>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QString>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QThreadPool>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/database.h"
#include "core/taskmanager.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreadertaglib.h"
#include "covermanager/albumcoverloader.h"
#include "collection/collectionmanager.h"
#include "collection/collectionbackend.h"
#include "collection/collectionwatcher.h"

#include "test_utils.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class CollectionManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // A database file, not an in-memory database, since the lyrics are checked against the collection in another thread, which has its own connection.
    ASSERT_TRUE(temp_dir_.isValid());
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    task_manager_ = make_shared<TaskManager>();

    // The tagreader runs in its own thread like in the application, and is deleted after the thread is stopped.
    tagreader_client_thread_ = new QThread();
    TagReaderClient *tagreader_client = new TagReaderClient();
    tagreader_client->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
    tagreader_client_ = SharedPtr<TagReaderClient>(tagreader_client, [](TagReaderClient *client) { Q_UNUSED(client) });
    tagreader_client_raw_ = tagreader_client;

    albumcover_loader_ = make_shared<AlbumCoverLoader>(tagreader_client_);
    collection_manager_ = new CollectionManager(database_, task_manager_, tagreader_client_, albumcover_loader_);
    collection_manager_->Init();

  }

  void TearDown() override {

    delete collection_manager_;
    albumcover_loader_.reset();
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_raw_;
    delete tagreader_client_thread_;
    tagreader_client_.reset();
    database_->Close();
    database_.reset();

  }

  // Exits the collection and then the tagreader, in the same order as the application.
  bool Exit() {

    if (!WaitForSignal([this]() { collection_manager_->Exit(); }, collection_manager_, &CollectionManager::ExitFinished)) return false;
    return WaitForSignal([this]() { tagreader_client_->ExitAsync(); }, tagreader_client_raw_, &TagReaderClient::ExitFinished);

  }

  template<typename Sender, typename Signal>
  static bool WaitForSignal(const std::function<void()> &start, Sender *sender, Signal signal) {

    QEventLoop loop;
    bool received = false;
    QObject::connect(sender, signal, &loop, [&loop, &received]() {
      received = true;
      loop.quit();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    start();
    if (!received) loop.exec();
    return received;

  }

  QTemporaryDir temp_dir_;
  SharedPtr<Database> database_;
  SharedPtr<TaskManager> task_manager_;
  QThread *tagreader_client_thread_ = nullptr;
  TagReaderClient *tagreader_client_raw_ = nullptr;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  CollectionManager *collection_manager_ = nullptr;

  // Returns if the collection watcher's rescan is paused, after the calls queued to the collection watcher's thread before it.
  bool WatcherRescanPaused() const {

    CollectionWatcher *watcher = collection_manager_->watcher();
    bool paused = false;
    QMetaObject::invokeMethod(watcher, [watcher, &paused]() { paused = watcher->rescan_paused(); }, Qt::BlockingQueuedConnection);
    return paused;

  }
};

TEST_F(CollectionManagerTest, ExitSavesDeferredPlaycountAndRating) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  Song song(Song::Source::Collection);
  song.set_url(QUrl::fromLocalFile(r.fileName()));
  song.set_filetype(Song::FileType::MPEG);
  song.set_playcount(7);
  song.set_rating(0.8F);

  // Saving the playcount and rating of the playing MP3 file is deferred until the song is no longer playing, playback is not stopped when exiting.
  collection_manager_->CurrentSongChanged(song);
  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsPlaycountChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));
  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsRatingChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));

  ASSERT_TRUE(Exit());

  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(r.fileName(), &saved_song).success());
  EXPECT_EQ(7U, saved_song.playcount());
  EXPECT_FLOAT_EQ(0.8F, saved_song.rating());

}

TEST_F(CollectionManagerTest, ExitSavesDeferredLyrics) {

  // The song is added to the collection directly, its directory is an empty collection directory, so the collection watcher doesn't find any songs when it scans it.
  SharedPtr<CollectionBackend> backend = collection_manager_->backend();
  ASSERT_TRUE(QDir().mkpath(temp_dir_.filePath(u"collection"_s)));
  backend->AddDirectory(temp_dir_.filePath(u"collection"_s));
  const QString filename = temp_dir_.filePath(u"01.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner));

  Song song(Song::Source::Collection);
  song.set_url(QUrl::fromLocalFile(filename));
  song.set_filetype(Song::FileType::MPEG);
  song.set_directory_id(1);
  song.set_title(u"Title"_s);
  song.set_mtime(1);
  song.set_ctime(1);
  song.set_filesize(1);
  backend->AddOrUpdateSongs(SongList() << song);
  const SongList songs = backend->GetSongsByUrl(song.url());
  ASSERT_EQ(1, songs.count());
  song = songs.first();

  // Saving the lyrics of the playing MP3 file is deferred until the song is no longer playing, playback is not stopped when exiting.
  collection_manager_->CurrentSongChanged(song);
  ASSERT_TRUE(QMetaObject::invokeMethod(&*backend, "UpdateSongLyrics", Qt::DirectConnection, Q_ARG(int, song.id()), Q_ARG(bool, false), Q_ARG(QString, u"Deferred lyrics"_s), Q_ARG(int, static_cast<int>(Song::StorageType::Database)), Q_ARG(bool, false), Q_ARG(bool, true)));

  ASSERT_TRUE(Exit());

  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(filename, &saved_song).success());
  EXPECT_EQ(u"Deferred lyrics"_s, saved_song.lyrics());

}

// Returns a CUE track of the file, CUE tracks share one media file.
Song CueTrack(const QString &filename, const int track) {

  Song song(Song::Source::Collection);
  song.set_url(QUrl::fromLocalFile(filename));
  song.set_filetype(Song::FileType::MPEG);
  song.set_cue_path(QFileInfo(filename).absolutePath() + u"/album.cue"_s);
  song.set_track(track);
  song.set_beginning_nanosec((track - 1) * 1000000000LL);
  song.set_playcount(7);
  song.set_rating(0.8F);
  return song;

}

// Regression test: the playcount and rating of a CUE track were saved to the media file, which is shared by all the tracks of the CUE sheet.
TEST_F(CollectionManagerTest, PlaycountAndRatingOfCueTrackAreNotSavedToFile) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  const Song song = CueTrack(r.fileName(), 2);

  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsPlaycountChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));
  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsRatingChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));

  ASSERT_TRUE(Exit());

  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(r.fileName(), &saved_song).success());
  EXPECT_EQ(0U, saved_song.playcount());
  EXPECT_FLOAT_EQ(-1.0F, saved_song.rating());

}

// The save of the playing CUE track is not deferred either, it's not saved.
TEST_F(CollectionManagerTest, DeferredPlaycountAndRatingOfCueTrackAreNotSavedToFile) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  const Song song = CueTrack(r.fileName(), 2);

  collection_manager_->CurrentSongChanged(song);
  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsPlaycountChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));
  ASSERT_TRUE(QMetaObject::invokeMethod(collection_manager_, "SongsRatingChanged", Qt::DirectConnection, Q_ARG(SongList, SongList() << song), Q_ARG(bool, true)));

  ASSERT_TRUE(Exit());

  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(r.fileName(), &saved_song).success());
  EXPECT_EQ(0U, saved_song.playcount());
  EXPECT_FLOAT_EQ(-1.0F, saved_song.rating());

}

// Regression test: saving the playcounts and ratings of all songs to the files saved them for the CUE tracks to the shared media file, the last track in the collection overwrote the others.
TEST_F(CollectionManagerTest, SyncDoesNotSaveCueTracksToFile) {

  SharedPtr<CollectionBackend> backend = collection_manager_->backend();
  ASSERT_TRUE(QDir().mkpath(temp_dir_.filePath(u"collection"_s)));
  backend->AddDirectory(temp_dir_.filePath(u"collection"_s));
  const QString cue_filename = temp_dir_.filePath(u"album.mp3"_s);
  const QString filename = temp_dir_.filePath(u"song.mp3"_s);
  for (const QString &file : {cue_filename, filename}) {
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, file));
    ASSERT_TRUE(QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner));
  }

  SongList songs;
  for (int track = 1; track <= 2; ++track) {
    Song song = CueTrack(cue_filename, track);
    song.set_title(u"Track %1"_s.arg(track));
    songs << song;
  }
  Song song(Song::Source::Collection);
  song.set_url(QUrl::fromLocalFile(filename));
  song.set_filetype(Song::FileType::MPEG);
  song.set_title(u"Song"_s);
  song.set_playcount(5);
  song.set_rating(0.6F);
  songs << song;
  for (Song &collection_song : songs) {
    collection_song.set_directory_id(1);
    collection_song.set_mtime(1);
    collection_song.set_ctime(1);
    collection_song.set_filesize(1);
  }
  backend->AddOrUpdateSongs(songs);
  ASSERT_EQ(3, backend->GetAllSongs().count());

  collection_manager_->SyncPlaycountAndRatingToFilesAsync();
  QThreadPool::globalInstance()->waitForDone();

  Song saved_cue_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(cue_filename, &saved_cue_song).success());
  EXPECT_EQ(0U, saved_cue_song.playcount());
  EXPECT_FLOAT_EQ(-1.0F, saved_cue_song.rating());

  // The other songs are still saved.
  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(filename, &saved_song).success());
  EXPECT_EQ(5U, saved_song.playcount());
  EXPECT_FLOAT_EQ(0.6F, saved_song.rating());

}

// The lyrics of a CUE track are only saved to the database, not to the media file, which is shared by all the tracks of the CUE sheet.
TEST_F(CollectionManagerTest, LyricsOfCueTrackAreNotSavedToFile) {

  SharedPtr<CollectionBackend> backend = collection_manager_->backend();
  ASSERT_TRUE(QDir().mkpath(temp_dir_.filePath(u"collection"_s)));
  backend->AddDirectory(temp_dir_.filePath(u"collection"_s));
  const QString filename = temp_dir_.filePath(u"album.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner));
  Song original_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(filename, &original_song).success());

  Song song = CueTrack(filename, 2);
  song.set_directory_id(1);
  song.set_title(u"Track 2"_s);
  song.set_mtime(1);
  song.set_ctime(1);
  song.set_filesize(1);
  backend->AddOrUpdateSongs(SongList() << song);
  const SongList songs = backend->GetAllSongs();
  ASSERT_EQ(1, songs.count());
  song = songs.first();

  // Like lyrics found automatically, with saving the lyrics to the files enabled.
  ASSERT_TRUE(QMetaObject::invokeMethod(&*backend, "UpdateSongLyrics", Qt::DirectConnection, Q_ARG(int, song.id()), Q_ARG(bool, false), Q_ARG(QString, u"Lyrics"_s), Q_ARG(int, static_cast<int>(Song::StorageType::Database)), Q_ARG(bool, false), Q_ARG(bool, true)));

  ASSERT_TRUE(Exit());

  Song saved_song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(filename, &saved_song).success());
  EXPECT_EQ(original_song.lyrics(), saved_song.lyrics());
  // The lyrics are still saved to the database.
  EXPECT_EQ(u"Lyrics"_s, backend->GetSongById(song.id()).lyrics());

}

// The pause and resume signals from the task manager can be received in another order than the tasks were started and finished when they are emitted from different threads.
// The collection watcher is only resumed when no task which blocks collection scans is running anymore.
TEST_F(CollectionManagerTest, WatcherPausedWhileBlockingTaskRuns) {

  const int delete_task_id = task_manager_->StartTask(u"Deleting files"_s);
  task_manager_->SetTaskBlocksCollectionScans(delete_task_id);
  EXPECT_TRUE(WatcherRescanPaused());

  const int organize_task_id = task_manager_->StartTask(u"Organizing files"_s);
  task_manager_->SetTaskBlocksCollectionScans(organize_task_id);
  task_manager_->SetTaskFinished(delete_task_id);
  EXPECT_TRUE(WatcherRescanPaused());

  // The resume signal for the finished delete is received after the pause signal for the organize, which is still running.
  Q_EMIT task_manager_->ResumeCollectionWatchers();
  EXPECT_TRUE(WatcherRescanPaused());

  task_manager_->SetTaskFinished(organize_task_id);
  EXPECT_FALSE(WatcherRescanPaused());

  // A pause signal received after the task finished doesn't pause the collection watcher again.
  Q_EMIT task_manager_->PauseCollectionWatchers();
  EXPECT_FALSE(WatcherRescanPaused());

  ASSERT_TRUE(Exit());

}

}  // namespace
