/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include "gtest_include.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QThread>
#include <QtDebug>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/memorydatabase.h"
#include "constants/timeconstants.h"
#include "collection/collectionbackend.h"
#include "collection/collectionlibrary.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;
using std::make_shared;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class CollectionBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_unique<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionLibrary::kSongsTable), QLatin1String(CollectionLibrary::kDirsTable), QLatin1String(CollectionLibrary::kSubdirsTable));
  }

  static Song MakeDummySong(int directory_id) {
    // Returns a valid song with all the required fields set
    Song ret;
    ret.set_directory_id(directory_id);
    ret.set_url(QUrl::fromLocalFile(u"foo.flac"_s));
    ret.set_mtime(1);
    ret.set_ctime(1);
    ret.set_filesize(1);
    return ret;
  }

  SharedPtr<Database> database_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  ScopedPtr<CollectionBackend> backend_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
};

TEST_F(CollectionBackendTest, EmptyDatabase) {

  // Check the database is empty to start with
  QStringList artists = backend_->GetAllArtists();
  EXPECT_TRUE(artists.isEmpty());

  CollectionBackend::AlbumList albums = backend_->GetAllAlbums();
  EXPECT_TRUE(albums.isEmpty());

}

TEST_F(CollectionBackendTest, AddDirectory) {

  QSignalSpy spy(&*backend_, &CollectionBackend::DirectoryAdded);

  backend_->AddDirectory(u"/tmp"_s);

  // Check the signal was emitted correctly
  ASSERT_EQ(1, spy.count());
  CollectionDirectory dir = spy[0][0].value<CollectionDirectory>();
  EXPECT_EQ(u"/tmp"_s, dir.path);
  EXPECT_EQ(1, dir.id);
  EXPECT_EQ(0, spy[0][1].value<CollectionSubdirectoryList>().size());

}

TEST_F(CollectionBackendTest, RemoveDirectory) {

  // Add a directory
  CollectionDirectory dir;
  dir.id = 1;
  dir.path = u"/tmp"_s;
  backend_->AddDirectory(dir.path);

  QSignalSpy spy(&*backend_, &CollectionBackend::DirectoryDeleted);

  // Remove the directory again
  backend_->RemoveDirectory(dir);

  // Check the signal was emitted correctly
  ASSERT_EQ(1, spy.count());
  dir = spy[0][0].value<CollectionDirectory>();
  EXPECT_EQ(u"/tmp"_s, dir.path);
  EXPECT_EQ(1, dir.id);

}

TEST_F(CollectionBackendTest, GetAlbumArtNonExistent) {}

// Test adding a single song to the database, then getting various information back about it.
class SingleSong : public CollectionBackendTest {
 protected:
  void SetUp() override {
    CollectionBackendTest::SetUp();

    // Add a directory - this will get ID 1
    backend_->AddDirectory(u"/tmp"_s);

    // Make a song in that directory
    song_ = MakeDummySong(1);
    song_.set_title(u"Title"_s);
    song_.set_artist(u"Artist"_s);
    song_.set_album(u"Album"_s);
    song_.set_url(QUrl::fromLocalFile(u"foo.flac"_s));
  }

  void AddDummySong() {
    QSignalSpy added_spy(&*backend_, &CollectionBackend::SongsAdded);
    QSignalSpy deleted_spy(&*backend_, &CollectionBackend::SongsDeleted);

    // Add the song
    backend_->AddOrUpdateSongs(SongList() << song_);

    // Check the correct signals were emitted
    EXPECT_EQ(0, deleted_spy.count());
    ASSERT_EQ(1, added_spy.count());

    SongList list = *(reinterpret_cast<SongList*>(added_spy[0][0].data()));
    ASSERT_EQ(1, list.count());
    EXPECT_EQ(song_.title(), list[0].title());
    EXPECT_EQ(song_.artist(), list[0].artist());
    EXPECT_EQ(song_.album(), list[0].album());
    EXPECT_EQ(1, list[0].id());
    EXPECT_EQ(1, list[0].directory_id());
  }

  Song song_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

};

TEST_F(SingleSong, GetSongWithNoAlbum) {

  song_.set_album(""_L1);
  AddDummySong();
  if (HasFatalFailure()) return;

  EXPECT_EQ(1, backend_->GetAllArtists().size());
  CollectionBackend::AlbumList albums = backend_->GetAllAlbums();
  EXPECT_EQ(1, albums.size());
  EXPECT_EQ(u"Artist"_s, albums[0].album_artist);
  EXPECT_EQ(""_L1, albums[0].album);

}

TEST_F(SingleSong, GetAllArtists) {

  AddDummySong();
  if (HasFatalFailure()) return;

  QStringList artists = backend_->GetAllArtists();
  ASSERT_EQ(1, artists.size());
  EXPECT_EQ(song_.artist(), artists[0]);

}

TEST_F(SingleSong, GetAllAlbums) {

  AddDummySong();
  if (HasFatalFailure()) return;

  CollectionBackend::AlbumList albums = backend_->GetAllAlbums();
  ASSERT_EQ(1, albums.size());
  EXPECT_EQ(song_.album(), albums[0].album);
  EXPECT_EQ(song_.artist(), albums[0].album_artist);

}

TEST_F(SingleSong, GetAlbumsByArtist) {

  AddDummySong();
  if (HasFatalFailure()) return;

  CollectionBackend::AlbumList albums = backend_->GetAlbumsByArtist(u"Artist"_s);
  ASSERT_EQ(1, albums.size());
  EXPECT_EQ(song_.album(), albums[0].album);
  EXPECT_EQ(song_.artist(), albums[0].album_artist);

}

TEST_F(SingleSong, GetAlbumArt) {

  AddDummySong();
  if (HasFatalFailure()) return;

  CollectionBackend::Album album = backend_->GetAlbumArt(u"Artist"_s, u"Album"_s);
  EXPECT_EQ(song_.album(), album.album);
  EXPECT_EQ(song_.effective_albumartist(), album.album_artist);

}

TEST_F(SingleSong, GetSongs) {

  AddDummySong();
  if (HasFatalFailure()) return;

  SongList songs = backend_->GetAlbumSongs(u"Artist"_s, u"Album"_s);
  ASSERT_EQ(1, songs.size());
  EXPECT_EQ(song_.album(), songs[0].album());
  EXPECT_EQ(song_.artist(), songs[0].artist());
  EXPECT_EQ(song_.title(), songs[0].title());
  EXPECT_EQ(1, songs[0].id());

}

TEST_F(SingleSong, GetSongById) {

  AddDummySong();
  if (HasFatalFailure()) return;

  Song song = backend_->GetSongById(1);
  EXPECT_EQ(song_.album(), song.album());
  EXPECT_EQ(song_.artist(), song.artist());
  EXPECT_EQ(song_.title(), song.title());
  EXPECT_EQ(1, song.id());

}

TEST_F(SingleSong, FindSongsInDirectory) {

  AddDummySong();
  if (HasFatalFailure()) return;

  SongList songs = backend_->FindSongsInDirectory(1);
  ASSERT_EQ(1, songs.size());
  EXPECT_EQ(song_.album(), songs[0].album());
  EXPECT_EQ(song_.artist(), songs[0].artist());
  EXPECT_EQ(song_.title(), songs[0].title());
  EXPECT_EQ(1, songs[0].id());

}

TEST_F(SingleSong, UpdateSong) {

  AddDummySong();
  if (HasFatalFailure()) return;

  Song new_song(song_);
  new_song.set_id(1);
  new_song.set_title(u"A different title"_s);

  QSignalSpy added_spy(&*backend_, &CollectionBackend::SongsAdded);
  QSignalSpy changed_spy(&*backend_, &CollectionBackend::SongsChanged);
  QSignalSpy deleted_spy(&*backend_, &CollectionBackend::SongsDeleted);

  backend_->AddOrUpdateSongs(SongList() << new_song);

  ASSERT_EQ(0, added_spy.size());
  ASSERT_EQ(1, changed_spy.size());
  ASSERT_EQ(0, deleted_spy.size());

  SongList songs_changed = *(reinterpret_cast<SongList*>(changed_spy[0][0].data()));
  ASSERT_EQ(1, songs_changed.size());
  EXPECT_EQ(u"A different title"_s, songs_changed[0].title());
  EXPECT_EQ(1, songs_changed[0].id());

}

TEST_F(SingleSong, DeleteSongs) {

  AddDummySong();
  if (HasFatalFailure()) return;

  Song new_song(song_);
  new_song.set_id(1);

  QSignalSpy deleted_spy(&*backend_, &CollectionBackend::SongsDeleted);

  backend_->DeleteSongs(SongList() << new_song);

  ASSERT_EQ(1, deleted_spy.size());

  SongList songs_deleted = *(reinterpret_cast<SongList*>(deleted_spy[0][0].data()));
  ASSERT_EQ(1, songs_deleted.size());
  EXPECT_EQ(u"Title"_s, songs_deleted[0].title());
  EXPECT_EQ(1, songs_deleted[0].id());

  // Check we can't retrieve that song any more
  Song song = backend_->GetSongById(1);
  EXPECT_FALSE(song.is_valid());
  EXPECT_EQ(-1, song.id());

  // And the artist or album shouldn't show up either
  QStringList artists = backend_->GetAllArtists();
  EXPECT_EQ(0, artists.size());

  CollectionBackend::AlbumList albums = backend_->GetAllAlbums();
  EXPECT_EQ(0, albums.size());

}

TEST_F(SingleSong, MarkSongsUnavailable) {

  AddDummySong();
  if (HasFatalFailure()) return;

  Song new_song(song_);
  new_song.set_id(1);

  QSignalSpy deleted_spy(&*backend_, &CollectionBackend::SongsDeleted);

  backend_->MarkSongsUnavailable(SongList() << new_song);

  ASSERT_EQ(1, deleted_spy.size());

  SongList songs_deleted = *(reinterpret_cast<SongList*>(deleted_spy[0][0].data()));
  ASSERT_EQ(1, songs_deleted.size());
  EXPECT_EQ(u"Title"_s, songs_deleted[0].title());
  EXPECT_EQ(1, songs_deleted[0].id());

  // Check the song is marked as deleted.
  Song song = backend_->GetSongById(1);
  EXPECT_TRUE(song.is_valid());
  EXPECT_TRUE(song.unavailable());

  // And the artist or album shouldn't show up either
  QStringList artists = backend_->GetAllArtists();
  EXPECT_EQ(0, artists.size());

  CollectionBackend::AlbumList albums = backend_->GetAllAlbums();
  EXPECT_EQ(0, albums.size());

}

class TestUrls : public CollectionBackendTest {
 protected:
  void SetUp() override {
    CollectionBackendTest::SetUp();
    backend_->AddDirectory(u"/mnt/music"_s);
  }
};

TEST_F(TestUrls, TestUrls) {

  QStringList strings = QStringList() << u"file:///mnt/music/01 - Pink Floyd - Echoes.flac"_s
                                      << u"file:///mnt/music/02 - Björn Afzelius - Det räcker nu.flac"_s
                                      << u"file:///mnt/music/03 - Vazelina Bilopphøggers - Bomull i øra.flac"_s
                                      << u"file:///mnt/music/Test !#$%&'()-@^_`{}~..flac"_s;

  const QList<QUrl> urls = QUrl::fromStringList(strings);
  SongList songs;
  songs.reserve(urls.count());
  for (const QUrl &url : urls) {

    EXPECT_EQ(url, QUrl::fromEncoded(url.toString(QUrl::FullyEncoded).toUtf8()));
    EXPECT_EQ(url.toString(QUrl::FullyEncoded), QString::fromLatin1(url.toEncoded()));

    Song song(Song::Source::Collection);
    song.set_directory_id(1);
    song.set_title(u"Test Title"_s);
    song.set_album(u"Test Album"_s);
    song.set_artist(u"Test Artist"_s);
    song.set_url(url);
    song.set_length_nanosec(kNsecPerSec);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_filesize(1);
    song.set_valid(true);

    songs << song;

  }

  QSignalSpy spy(&*backend_, &CollectionBackend::SongsAdded);

  backend_->AddOrUpdateSongs(songs);
  if (HasFatalFailure()) return;

  ASSERT_EQ(1, spy.count());
  SongList new_songs = spy[0][0].value<SongList>();
  EXPECT_EQ(new_songs.count(), strings.count());

  for (const QUrl &url : urls) {

    songs = backend_->GetSongsByUrl(url);
    EXPECT_EQ(1, songs.count());
    if (songs.count() < 1) continue;

    Song new_song = songs.first();
    EXPECT_TRUE(new_song.is_valid());
    EXPECT_EQ(new_song.url(), url);

    new_song = backend_->GetSongByUrl(url);
    EXPECT_EQ(1, songs.count());
    if (songs.count() < 1) continue;

    EXPECT_TRUE(new_song.is_valid());
    EXPECT_EQ(new_song.url(), url);

    QSqlDatabase db(database_->Connect());
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT url FROM %1 WHERE url = :url").arg(QLatin1String(CollectionLibrary::kSongsTable)));

    q.bindValue(u":url"_s, url.toString(QUrl::FullyEncoded));
    EXPECT_TRUE(q.exec());

    while (q.next()) {
      EXPECT_EQ(url, q.value(0).toUrl());
      EXPECT_EQ(url, QUrl::fromEncoded(q.value(0).toByteArray()));
    }

  }

}

class UpdateSongsBySongID : public CollectionBackendTest {
 protected:
  void SetUp() override {
    CollectionBackendTest::SetUp();
    backend_->AddDirectory(u"/mnt/music"_s);
  }
};

TEST_F(UpdateSongsBySongID, UpdateSongsBySongID) {

    const QStringList song_ids = QStringList() << u"song1"_s
                                               << u"song2"_s
                                               << u"song3"_s
                                               << u"song4"_s
                                               << u"song5"_s
                                               << u"song6"_s;

  { // Add songs
    SongMap songs;

    for (const QString &song_id : song_ids) {

      QUrl url;
      url.setScheme(u"file"_s);
      url.setPath(u"/music/"_s + song_id);

      Song song(Song::Source::Collection);
      song.set_song_id(song_id);
      song.set_directory_id(1);
      song.set_title(u"Test Title "_s + song_id);
      song.set_album(u"Test Album"_s);
      song.set_artist(u"Test Artist"_s);
      song.set_url(url);
      song.set_length_nanosec(kNsecPerSec);
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_valid(true);

      songs.insert(song_id, song);

    }

    QSignalSpy spy(&*backend_, &CollectionBackend::SongsAdded);

    backend_->UpdateSongsBySongID(songs);

    ASSERT_EQ(1, spy.count());
    SongList new_songs = spy[0][0].value<SongList>();
    EXPECT_EQ(new_songs.count(), song_ids.count());
    EXPECT_EQ(song_ids[0], new_songs[0].song_id());
    EXPECT_EQ(song_ids[1], new_songs[1].song_id());
    EXPECT_EQ(song_ids[2], new_songs[2].song_id());
    EXPECT_EQ(song_ids[3], new_songs[3].song_id());
    EXPECT_EQ(song_ids[4], new_songs[4].song_id());
    EXPECT_EQ(song_ids[5], new_songs[5].song_id());

  }

  {  // Check that all songs are added.

    SongMap songs;
    {
      QSqlDatabase db(database_->Connect());
      CollectionQuery query(db, QLatin1String(CollectionLibrary::kSongsTable));
      EXPECT_TRUE(backend_->ExecCollectionQuery(&query, songs));
    }

    EXPECT_EQ(songs.count(), song_ids.count());

    for (SongMap::const_iterator it = songs.constBegin() ; it != songs.constEnd() ; ++it) {
      EXPECT_EQ(it.key(), it.value().song_id());
    }

    for (const QString &song_id : song_ids) {
      EXPECT_TRUE(songs.contains(song_id));
    }

  }

  {  // Remove some songs
    QSignalSpy spy1(&*backend_, &CollectionBackend::SongsAdded);
    QSignalSpy spy2(&*backend_, &CollectionBackend::SongsDeleted);

    SongMap songs;

    const QStringList song_ids2 = QStringList() << u"song1"_s
                                                << u"song4"_s
                                                << u"song5"_s
                                                << u"song6"_s;

    for (const QString &song_id : song_ids2) {

      QUrl url;
      url.setScheme(u"file"_s);
      url.setPath(u"/music/"_s + song_id);

      Song song(Song::Source::Collection);
      song.set_song_id(song_id);
      song.set_directory_id(1);
      song.set_title(u"Test Title "_s + song_id);
      song.set_album(u"Test Album"_s);
      song.set_artist(u"Test Artist"_s);
      song.set_url(url);
      song.set_length_nanosec(kNsecPerSec);
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_valid(true);

      songs.insert(song_id, song);

    }

    backend_->UpdateSongsBySongID(songs);

    ASSERT_EQ(0, spy1.count());
    ASSERT_EQ(1, spy2.count());
    SongList deleted_songs = spy2[0][0].value<SongList>();
    EXPECT_EQ(deleted_songs.count(), 2);
    EXPECT_EQ(deleted_songs[0].song_id(), u"song2"_s);
    EXPECT_EQ(deleted_songs[1].song_id(), u"song3"_s);

  }

  {  // Update some songs
    QSignalSpy spy1(&*backend_, &CollectionBackend::SongsDeleted);
    QSignalSpy spy2(&*backend_, &CollectionBackend::SongsAdded);
    QSignalSpy spy3(&*backend_, &CollectionBackend::SongsChanged);

    SongMap songs;

    const QStringList song_ids2 = QStringList() << u"song1"_s
                                                << u"song4"_s
                                                << u"song5"_s
                                                << u"song6"_s;

    for (const QString &song_id : song_ids2) {

      QUrl url;
      url.setScheme(u"file"_s);
      url.setPath(u"/music/"_s + song_id);

      Song song(Song::Source::Collection);
      song.set_song_id(song_id);
      song.set_directory_id(1);
      song.set_title(u"Test Title "_s + song_id);
      song.set_album(u"Test Album"_s);
      song.set_artist(u"Test Artist"_s);
      song.set_url(url);
      song.set_length_nanosec(kNsecPerSec);
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_valid(true);

      songs.insert(song_id, song);

    }

    songs[u"song1"_s].set_artist(u"New artist"_s);
    songs[u"song6"_s].set_artist(u"New artist"_s);

    backend_->UpdateSongsBySongID(songs);

    ASSERT_EQ(0, spy1.count());
    ASSERT_EQ(0, spy2.count());
    ASSERT_EQ(1, spy3.count());

    SongList changed_songs = spy3[0][0].value<SongList>();
    EXPECT_EQ(changed_songs.count(), 2);
    EXPECT_EQ(changed_songs[0].song_id(), u"song1"_s);
    EXPECT_EQ(changed_songs[1].song_id(), u"song6"_s);

  }

}

} // namespace
