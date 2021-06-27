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

#include <gtest/gtest.h>

#include <QMap>
#include <QString>
#include <QUrl>
#include <QThread>
#include <QSignalSpy>
#include <QtDebug>

#include "core/logging.h"
#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "core/database.h"
#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"

using std::make_unique;
using std::make_shared;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class CollectionModelTest : public ::testing::Test {
 public:
  CollectionModelTest() : collection_filter_(nullptr), added_dir_(false) {}

 protected:
  void SetUp() override {
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(SCollection::kSongsTable), QLatin1String(SCollection::kDirsTable), QLatin1String(SCollection::kSubdirsTable));
    model_ = make_unique<CollectionModel>(backend_, nullptr);
    collection_filter_ = model_->filter();

    added_dir_ = false;

  }

  Song AddSong(Song &song) {
    song.set_directory_id(1);
    if (song.mtime() == 0) song.set_mtime(1);
    if (song.ctime() == 0) song.set_ctime(1);
    if (song.url().isEmpty()) song.set_url(QUrl(QStringLiteral("file:///tmp/foo")));
    if (song.filesize() == -1) song.set_filesize(1);

    if (!added_dir_) {
      backend_->AddDirectory(QStringLiteral("/tmp"));
      added_dir_ = true;
    }

    QEventLoop loop;
    QObject::connect(&*model_, &CollectionModel::rowsInserted, &loop, &QEventLoop::quit);
    backend_->AddOrUpdateSongs(SongList() << song);
    loop.exec();

    return song;
  }

  Song AddSong(const QString &title, const QString &artist, const QString &album, const int length) {
    Song song;
    song.Init(title, artist, album, length);
    song.set_mtime(0);
    song.set_ctime(0);
    return AddSong(song);
  }

  SharedPtr<Database> database_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  SharedPtr<CollectionBackend> backend_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  ScopedPtr<CollectionModel> model_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  CollectionFilter *collection_filter_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  bool added_dir_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
};

TEST_F(CollectionModelTest, Initialization) {
  EXPECT_EQ(0, model_->rowCount(QModelIndex()));
}

TEST_F(CollectionModelTest, WithInitialArtists) {

  AddSong(QStringLiteral("Title"), QStringLiteral("Artist 1"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("Artist 2"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("Foo"), QStringLiteral("Album"), 123);

  ASSERT_EQ(5, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(QStringLiteral("A"), collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("Artist 1"), collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("Artist 2"), collection_filter_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("F"), collection_filter_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("Foo"), collection_filter_->index(4, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, CompilationAlbums) {

  Song song;
  song.Init(QStringLiteral("Title"), QStringLiteral("Artist"), QStringLiteral("Album"), 123);
  song.set_compilation(true);
  song.set_mtime(0);
  song.set_ctime(0);

  AddSong(song);

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));

  QModelIndex va_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ(QStringLiteral("Various artists"), va_index.data().toString());
  EXPECT_TRUE(model_->hasChildren(va_index));
  ASSERT_EQ(model_->rowCount(va_index), 1);

  QModelIndex album_index = model_->index(0, 0, va_index);
  EXPECT_EQ(model_->data(album_index).toString(), QStringLiteral("Album"));
  EXPECT_TRUE(model_->hasChildren(album_index));

}

TEST_F(CollectionModelTest, NumericHeaders) {

  AddSong(QStringLiteral("Title"), QStringLiteral("1artist"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("2artist"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("0artist"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("zartist"), QStringLiteral("Album"), 123);

  ASSERT_EQ(6, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(QStringLiteral("0-9"), collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("0artist"), collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("1artist"), collection_filter_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("2artist"), collection_filter_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("Z"), collection_filter_->index(4, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("zartist"), collection_filter_->index(5, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, MixedCaseHeaders) {

  AddSong(QStringLiteral("Title"), QStringLiteral("Artist"), QStringLiteral("Album"), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("artist"), QStringLiteral("Album"), 123);

  ASSERT_EQ(3, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(QStringLiteral("A"), collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("Artist"), collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(QStringLiteral("artist"), collection_filter_->index(2, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, UnknownArtists) {

  AddSong(QStringLiteral("Title"), QLatin1String(""), QStringLiteral("Album"), 123);

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));
  QModelIndex unknown_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ(QStringLiteral("Unknown"), unknown_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(unknown_index));
  EXPECT_EQ(QStringLiteral("Album"), model_->index(0, 0, unknown_index).data().toString());

}

TEST_F(CollectionModelTest, UnknownAlbums) {

  AddSong(QStringLiteral("Title"), QStringLiteral("Artist"), QLatin1String(""), 123);
  AddSong(QStringLiteral("Title"), QStringLiteral("Artist"), QStringLiteral("Album"), 123);

  QModelIndex artist_index = model_->index(1, 0, QModelIndex());
  EXPECT_EQ(artist_index.isValid(), true);
  ASSERT_EQ(2, model_->rowCount(artist_index));

  QModelIndex unknown_album_index = model_->index(0, 0, artist_index);
  QModelIndex real_album_index = model_->index(1, 0, artist_index);

  EXPECT_EQ(QStringLiteral("Unknown"), unknown_album_index.data().toString());
  EXPECT_EQ(QStringLiteral("Album"), real_album_index.data().toString());

}

TEST_F(CollectionModelTest, VariousArtistSongs) {

  SongList songs;
  for (int i=0 ; i < 4 ; ++i) {
    QString n = QString::number(i+1);
    Song song;
    song.Init(QStringLiteral("Title ") + n, QStringLiteral("Artist ") + n, QStringLiteral("Album"), 0);
    song.set_mtime(0);
    song.set_ctime(0);
    songs << song;  // clazy:exclude=reserve-candidates
  }

  // Different ways of putting songs in "Various Artist".  Make sure they all work
  songs[0].set_compilation_detected(true);
  songs[1].set_compilation(true);
  songs[2].set_compilation_on(true);
  songs[3].set_compilation_detected(true); songs[3].set_artist(QStringLiteral("Various Artists"));

  for (int i=0 ; i < 4 ; ++i)
    AddSong(songs[i]);

  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(4, model_->rowCount(album_index));

  EXPECT_EQ(QStringLiteral("Artist 1 - Title 1"), model_->index(0, 0, album_index).data().toString());
  EXPECT_EQ(QStringLiteral("Artist 2 - Title 2"), model_->index(1, 0, album_index).data().toString());
  EXPECT_EQ(QStringLiteral("Artist 3 - Title 3"), model_->index(2, 0, album_index).data().toString());
  EXPECT_EQ(QStringLiteral("Title 4"), model_->index(3, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveSongs) {

  Song one = AddSong(QStringLiteral("Title 1"), QStringLiteral("Artist"), QStringLiteral("Album"), 123); one.set_id(1);
  Song two = AddSong(QStringLiteral("Title 2"), QStringLiteral("Artist"), QStringLiteral("Album"), 123); two.set_id(2);
  AddSong(QStringLiteral("Title 3"), QStringLiteral("Artist"), QStringLiteral("Album"), 123);

  QModelIndex artist_index = model_->index(1, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(3, model_->rowCount(album_index));

  // Remove the first two songs
  QSignalSpy spy_preremove(&*model_, &CollectionModel::rowsAboutToBeRemoved);
  QSignalSpy spy_remove(&*model_, &CollectionModel::rowsRemoved);
  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  QEventLoop loop;
  QObject::connect(&*model_, &CollectionModel::rowsRemoved, &loop, &QEventLoop::quit);
  backend_->DeleteSongs(SongList() << one << two);
  loop.exec();

  ASSERT_EQ(2, spy_preremove.count());
  ASSERT_EQ(2, spy_remove.count());
  ASSERT_EQ(0, spy_reset.count());

  artist_index = model_->index(1, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));
  album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(1, model_->rowCount(album_index));
  EXPECT_EQ(QStringLiteral("Title 3"), model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveEmptyAlbums) {

  Song one = AddSong(QStringLiteral("Title 1"), QStringLiteral("Artist"), QStringLiteral("Album 1"), 123); one.set_id(1);
  Song two = AddSong(QStringLiteral("Title 2"), QStringLiteral("Artist"), QStringLiteral("Album 2"), 123); two.set_id(2);
  Song three = AddSong(QStringLiteral("Title 3"), QStringLiteral("Artist"), QStringLiteral("Album 2"), 123); three.set_id(3);

  QModelIndex artist_index = model_->index(1, 0, QModelIndex());
  ASSERT_EQ(2, model_->rowCount(artist_index));

  // Remove one song from each album
  QEventLoop loop;
  QObject::connect(&*model_, &CollectionModel::rowsRemoved, &loop, &QEventLoop::quit);
  backend_->DeleteSongs(SongList() << one << two);
  loop.exec();

  // Check the model
  artist_index = model_->index(1, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  EXPECT_EQ(QStringLiteral("Album 2"), album_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(album_index));
  EXPECT_EQ(QStringLiteral("Title 3"), model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveEmptyArtists) {

  Song one = AddSong(QStringLiteral("Title"), QStringLiteral("Artist"), QStringLiteral("Album"), 123); one.set_id(1);

  QModelIndex artist_index = model_->index(1, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(1, model_->rowCount(album_index));

  // The artist header is there too right?
  ASSERT_EQ(2, model_->rowCount(QModelIndex()));

  // Remove the song
  QEventLoop loop;
  QObject::connect(&*model_, &CollectionModel::rowsRemoved, &loop, &QEventLoop::quit);
  backend_->DeleteSongs(SongList() << one);
  loop.exec();

  // Everything should be gone - even the artist header
  ASSERT_EQ(0, model_->rowCount(QModelIndex()));

}

// Test to check that the container nodes are created identical and unique all through the model with all possible collection groupings.
// model1 - Nodes are created from a complete reset done through lazy-loading.
// model2 - Initial container nodes are created in SongsAdded.
// model3 - All container nodes are created in SongsAdded.

// WARNING: This test can take up to 30 minutes to complete.
#if 0
TEST_F(CollectionModelTest, TestContainerNodes) {

  SongList songs;
  int year = 1960;
  // Add some normal albums.
  for (int artist_number = 1; artist_number <= 3 ; ++artist_number) {
    Song song(Song::Source::Collection);
    song.set_artist(QStringLiteral("Artist %1").arg(artist_number));
    song.set_composer(QStringLiteral("Composer %1").arg(artist_number));
    song.set_performer(QStringLiteral("Performer %1").arg(artist_number));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    for (int album_number = 1; album_number <= 3 ; ++album_number) {
      if (year > 2020) year = 1960;
      song.set_album(QStringLiteral("Artist %1 - Album %2").arg(artist_number).arg(album_number));
      song.set_album_id(QString::number(album_number));
      song.set_year(year++);
      song.set_genre(QStringLiteral("Rock"));
      for (int song_number = 1; song_number <= 5 ; ++song_number) {
        song.set_url(QUrl(QStringLiteral("file:///mnt/music/Artist %1/Album %2/%3 - artist song-n-%3").arg(artist_number).arg(album_number).arg(song_number)));
        song.set_title(QStringLiteral("Title %1").arg(song_number));
        song.set_track(song_number);
        songs << song;
      }
    }
  }

  // Add some albums with 'album artist'.
  for (int album_artist_number = 1; album_artist_number <= 3 ; ++album_artist_number) {
    Song song(Song::Source::Collection);
    song.set_albumartist(QStringLiteral("Album Artist %1").arg(album_artist_number));
    song.set_composer(QStringLiteral("Composer %1").arg(album_artist_number));
    song.set_performer(QStringLiteral("Performer %1").arg(album_artist_number));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    for (int album_number = 1; album_number <= 3 ; ++album_number) {
      if (year > 2020) year = 1960;
      song.set_album(QStringLiteral("Album Artist %1 - Album %2").arg(album_artist_number).arg(album_number));
      song.set_album_id(QString::number(album_number));
      song.set_year(year++);
      song.set_genre(QStringLiteral("Rock"));
      int artist_number = 1;
      for (int song_number = 1; song_number <= 5 ; ++song_number) {
        song.set_url(QUrl(QStringLiteral("file:///mnt/music/Album Artist %1/Album %2/%3 - album artist song-n-%3").arg(album_artist_number).arg(album_number).arg(QString::number(song_number))));
        song.set_title(QStringLiteral("Title ") + QString::number(song_number));
        song.set_track(song_number);
        song.set_artist(QStringLiteral("Artist ") + QString::number(artist_number));
        songs << song;
        ++artist_number;
      }
    }
  }

  // Add some compilation albums.
  for (int album_number = 1; album_number <= 3 ; ++album_number) {
    if (year > 2020) year = 1960;
    Song song(Song::Source::Collection);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_album(QStringLiteral("Compilation Album %1").arg(album_number));
    song.set_album_id(QString::number(album_number));
    song.set_year(year++);
    song.set_genre(QStringLiteral("Pop"));
    song.set_compilation(true);
    int artist_number = 1;
    for (int song_number = 1; song_number <= 4 ; ++song_number) {
      song.set_url(QUrl(QStringLiteral("file:///mnt/music/Compilation Artist %1/Compilation Album %2/%3 - compilation song-n-%3").arg(artist_number).arg(album_number).arg(QString::number(song_number))));
      song.set_artist(QStringLiteral("Compilation Artist %1").arg(artist_number));
      song.set_composer(QStringLiteral("Composer %1").arg(artist_number));
      song.set_performer(QStringLiteral("Performer %1").arg(artist_number));
      song.set_title(QStringLiteral("Title %1").arg(song_number));
      song.set_track(song_number);
      songs << song;
      ++artist_number;
    }
  }

  // Songs with only title
  {
    Song song(Song::Source::Collection);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_url(QUrl(QStringLiteral("file:///mnt/music/no album song 1/song-only-1")));
    song.set_title(QStringLiteral("Only Title 1"));
    songs << song;
    song.set_url(QUrl(QStringLiteral("file:///mnt/music/no album song 2/song-only-2")));
    song.set_title(QStringLiteral("Only Title 2"));
    songs << song;
  }

  // Song with only artist, album and title.
  {
    Song song(Song::Source::Collection);
    song.set_url(QUrl(QStringLiteral("file:///tmp/artist-album-title-song")));
    song.set_artist(QStringLiteral("Not Only Artist"));
    song.set_album(QStringLiteral("Not Only Album"));
    song.set_title(QStringLiteral("Not Only Title"));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  // Add possible Various artists conflicting songs.
  {
    Song song(Song::Source::Collection);
    song.set_url(QUrl(QStringLiteral("file:///tmp/song-va-conflicting-1")));
    song.set_artist(QStringLiteral("Various artists"));
    song.set_album(QStringLiteral("VA Album"));
    song.set_title(QStringLiteral("VA Title"));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  {
    Song song(Song::Source::Collection);
    song.set_url(QUrl(QStringLiteral("file:///tmp/song-va-conflicting-2")));
    song.set_artist(QStringLiteral("Various artists"));
    song.set_albumartist(QStringLiteral("Various artists"));
    song.set_album(QStringLiteral("VA Album"));
    song.set_title(QStringLiteral("VA Title"));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  {
    Song song(Song::Source::Collection);
    song.set_url(QUrl(QStringLiteral("file:///tmp/song-va-conflicting-3")));
    song.set_albumartist(QStringLiteral("Various artists"));
    song.set_album(QStringLiteral("VA Album"));
    song.set_title(QStringLiteral("VA Title"));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  // Albums with Album ID.
  for (int album_id = 0; album_id <= 2 ; ++album_id) {
    Song song(Song::Source::Collection);
    song.set_url(QUrl(QStringLiteral("file:///tmp/song-with-album-id-1")));
    song.set_artist(QStringLiteral("Artist with Album ID"));
    song.set_album(QStringLiteral("Album %1 with Album ID").arg(album_id));
    song.set_album_id(QStringLiteral("Album ID %1").arg(album_id));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType::FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    for (int i = 0; i <= 3 ; ++i) {
      song.set_title(QStringLiteral("Title %1 %2").arg(album_id).arg(i));
      song.set_track(i);
      songs << song;
    }
  }

  for (int f = static_cast<int>(CollectionModel::GroupBy::None) + 1 ; f < static_cast<int>(CollectionModel::GroupBy::GroupByCount) ; ++f) {
    for (int s = static_cast<int>(CollectionModel::GroupBy::None) ; s < static_cast<int>(CollectionModel::GroupBy::GroupByCount) ; ++s) {
      for (int t = static_cast<int>(CollectionModel::GroupBy::None) ; t < static_cast<int>(CollectionModel::GroupBy::GroupByCount) ; ++t) {

        qLog(Debug) << "Testing collection model grouping: " << f << s << t;

        SharedPtr<Database> database1;
        SharedPtr<Database> database2;
        SharedPtr<Database> database3;
        SharedPtr<CollectionBackend> backend1;
        SharedPtr<CollectionBackend> backend2;
        SharedPtr<CollectionBackend> backend3;
        ScopedPtr<CollectionModel> model1;
        ScopedPtr<CollectionModel> model2;
        ScopedPtr<CollectionModel> model3;

        database1 = make_unique<MemoryDatabase>(nullptr);
        database2 = make_unique<MemoryDatabase>(nullptr);
        database3 = make_unique<MemoryDatabase>(nullptr);
        backend1 = make_shared<CollectionBackend>();
        backend2= make_shared<CollectionBackend>();
        backend3 = make_shared<CollectionBackend>();
        backend1->Init(database1, nullptr, Song::Source::Collection, QLatin1String(SCollection::kSongsTable), QLatin1String(SCollection::kDirsTable), QLatin1String(SCollection::kSubdirsTable));
        backend2->Init(database2, nullptr, Song::Source::Collection, QLatin1String(SCollection::kSongsTable), QLatin1String(SCollection::kDirsTable), QLatin1String(SCollection::kSubdirsTable));
        backend3->Init(database3, nullptr, Song::Source::Collection, QLatin1String(SCollection::kSongsTable), QLatin1String(SCollection::kDirsTable), QLatin1String(SCollection::kSubdirsTable));
        model1 = make_unique<CollectionModel>(backend1, nullptr);
        model2 = make_unique<CollectionModel>(backend2, nullptr);
        model3 = make_unique<CollectionModel>(backend3, nullptr);

        backend1->AddDirectory(QStringLiteral("/mnt/music"));
        backend2->AddDirectory(QStringLiteral("/mnt/music"));
        backend3->AddDirectory(QStringLiteral("/mut/music"));

        model1->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));
        model2->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));
        model3->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));

        QSignalSpy model1_update(&*model1, &CollectionModel::SongsAdded);
        QSignalSpy model2_update(&*model2, &CollectionModel::SongsAdded);
        QSignalSpy model3_update(&*model3, &CollectionModel::SongsAdded);

        {
          QEventLoop event_loop;
          QObject::connect(&*model1, &CollectionModel::rowsInserted, &event_loop, &QEventLoop::quit);
          backend1->AddOrUpdateSongs(songs);
          event_loop.exec();
        }

        {
          QEventLoop event_loop;
          QObject::connect(&*model2, &CollectionModel::rowsInserted, &event_loop, &QEventLoop::quit);
          backend2->AddOrUpdateSongs(songs);
          event_loop.exec();
        }

        {
          QEventLoop event_loop;
          QObject::connect(&*model3, &CollectionModel::rowsInserted, &event_loop, &QEventLoop::quit);
          backend3->AddOrUpdateSongs(songs);
          event_loop.exec();
        }

        ASSERT_EQ(model1->song_nodes().count(), songs.count());
        ASSERT_EQ(model2->song_nodes().count(), songs.count());
        ASSERT_EQ(model3->song_nodes().count(), songs.count());

        model1->Init();

        model1->ExpandAll();
        model2->ExpandAll();
        // All nodes in model3 should be created already.

        ASSERT_EQ(model1->song_nodes().count(), songs.count());
        ASSERT_EQ(model2->song_nodes().count(), songs.count());
        ASSERT_EQ(model3->song_nodes().count(), songs.count());

        // Container nodes for all models should now be identical.
        for (int i = 0 ; i < 3 ; ++i) {
          for (CollectionItem *node : model1->container_nodes(i).values()) {
            ASSERT_TRUE(model2->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }
          for (CollectionItem *node : model1->container_nodes(i).values()) {
            ASSERT_TRUE(model3->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }

          for (CollectionItem *node : model2->container_nodes(i).values()) {
            ASSERT_TRUE(model1->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }
          for (CollectionItem *node : model2->container_nodes(i).values()) {
            ASSERT_TRUE(model3->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }

          for (CollectionItem *node : model3->container_nodes(i).values()) {
            ASSERT_TRUE(model1->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }
          for (CollectionItem *node : model3->container_nodes(i).values()) {
            ASSERT_TRUE(model2->container_nodes(i).keys().contains(node->key));
            CollectionItem *node2 = model2->container_nodes(i)[node->key];
            ASSERT_EQ(node->key, node2->key);
            ASSERT_EQ(node->display_text, node2->display_text);
            ASSERT_EQ(node->sort_text, node2->sort_text);
          }
        }

        QSignalSpy database_reset_1(&*backend1, &CollectionBackend::DatabaseReset);
        QSignalSpy database_reset_2(&*backend2, &CollectionBackend::DatabaseReset);
        QSignalSpy database_reset_3(&*backend3, &CollectionBackend::DatabaseReset);

        {
          QEventLoop event_loop;
          QObject::connect(&*model1, &CollectionModel::modelReset, &event_loop, &QEventLoop::quit);
          backend1->DeleteAll();
          event_loop.exec();
        }

        {
          QEventLoop event_loop;
          QObject::connect(&*model2, &CollectionModel::modelReset, &event_loop, &QEventLoop::quit);
          backend2->DeleteAll();
          event_loop.exec();
        }

        {
          QEventLoop event_loop;
          QObject::connect(&*model3, &CollectionModel::modelReset, &event_loop, &QEventLoop::quit);
          backend3->DeleteAll();
          event_loop.exec();
        }

        ASSERT_EQ(database_reset_1.count(), 1);
        ASSERT_EQ(database_reset_2.count(), 1);
        ASSERT_EQ(database_reset_3.count(), 1);

        // Make sure all nodes are deleted.

        ASSERT_EQ(model1->container_nodes(0).count(), 0);
        ASSERT_EQ(model1->container_nodes(1).count(), 0);
        ASSERT_EQ(model1->container_nodes(2).count(), 0);

        ASSERT_EQ(model2->container_nodes(0).count(), 0);
        ASSERT_EQ(model2->container_nodes(1).count(), 0);
        ASSERT_EQ(model2->container_nodes(2).count(), 0);

        ASSERT_EQ(model3->container_nodes(0).count(), 0);
        ASSERT_EQ(model3->container_nodes(1).count(), 0);
        ASSERT_EQ(model3->container_nodes(2).count(), 0);

        ASSERT_EQ(model1->song_nodes().count(), 0);
        ASSERT_EQ(model2->song_nodes().count(), 0);
        ASSERT_EQ(model3->song_nodes().count(), 0);

        ASSERT_EQ(model1->divider_nodes_count(), 0);
        ASSERT_EQ(model2->divider_nodes_count(), 0);
        ASSERT_EQ(model3->divider_nodes_count(), 0);

        backend1->Close();
        backend2->Close();
        backend3->Close();

      }
    }
  }

}
#endif

}  // namespace
