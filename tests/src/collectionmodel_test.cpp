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
#include <QSortFilterProxyModel>
#include <QtDebug>

#include "test_utils.h"

#include "core/logging.h"
#include "core/database.h"
#include "collection/collectionmodel.h"
#include "collection/collectionbackend.h"
#include "collection/collection.h"

namespace {

class CollectionModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_.reset(new MemoryDatabase(nullptr));
    backend_.reset(new CollectionBackend);
    backend_->Init(database_.get(), Song::Source_Collection, SCollection::kSongsTable, SCollection::kDirsTable, SCollection::kSubdirsTable, SCollection::kFtsTable);
    model_.reset(new CollectionModel(backend_.get(), nullptr));

    added_dir_ = false;

    model_sorted_.reset(new QSortFilterProxyModel);
    model_sorted_->setSourceModel(model_.get());
    model_sorted_->setSortRole(CollectionModel::Role_SortText);
    model_sorted_->setDynamicSortFilter(true);
    model_sorted_->sort(0);

  }

  Song AddSong(Song &song) {
    song.set_directory_id(1);
    if (song.mtime() == 0) song.set_mtime(1);
    if (song.ctime() == 0) song.set_ctime(1);
    if (song.url().isEmpty()) song.set_url(QUrl("file:///tmp/foo"));
    if (song.filesize() == -1) song.set_filesize(1);

    if (!added_dir_) {
      backend_->AddDirectory("/tmp");
      added_dir_ = true;
    }

    backend_->AddOrUpdateSongs(SongList() << song);
    return song;
  }

  Song AddSong(const QString &title, const QString &artist, const QString &album, const int length) {
    Song song;
    song.Init(title, artist, album, length);
    song.set_mtime(0);
    song.set_ctime(0);
    return AddSong(song);
  }

  std::shared_ptr<Database> database_;
  std::unique_ptr<CollectionBackend> backend_;
  std::unique_ptr<CollectionModel> model_;
  std::unique_ptr<QSortFilterProxyModel> model_sorted_;

  bool added_dir_;
};

TEST_F(CollectionModelTest, Initialisation) {
  EXPECT_EQ(0, model_->rowCount(QModelIndex()));
}

TEST_F(CollectionModelTest, WithInitialArtists) {

  AddSong("Title", "Artist 1", "Album", 123);
  AddSong("Title", "Artist 2", "Album", 123);
  AddSong("Title", "Foo", "Album", 123);
  model_->Init(false);

  ASSERT_EQ(5, model_sorted_->rowCount(QModelIndex()));
  EXPECT_EQ("A", model_sorted_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ("Artist 1", model_sorted_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ("Artist 2", model_sorted_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ("F", model_sorted_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ("Foo", model_sorted_->index(4, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, CompilationAlbums) {

  Song song;
  song.Init("Title", "Artist", "Album", 123);
  song.set_compilation(true);
  song.set_mtime(0);
  song.set_ctime(0);

  AddSong(song);
  model_->Init(false);
  model_->fetchMore(model_->index(0, 0));

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));

  QModelIndex va_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ("Various artists", va_index.data().toString());
  EXPECT_TRUE(model_->hasChildren(va_index));
  ASSERT_EQ(model_->rowCount(va_index), 1);

  QModelIndex album_index = model_->index(0, 0, va_index);
  EXPECT_EQ(model_->data(album_index).toString(), "Album");
  EXPECT_TRUE(model_->hasChildren(album_index));

}

TEST_F(CollectionModelTest, NumericHeaders) {

  AddSong("Title", "1artist", "Album", 123);
  AddSong("Title", "2artist", "Album", 123);
  AddSong("Title", "0artist", "Album", 123);
  AddSong("Title", "zartist", "Album", 123);
  model_->Init(false);

  ASSERT_EQ(6, model_sorted_->rowCount(QModelIndex()));
  EXPECT_EQ("0-9", model_sorted_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ("0artist", model_sorted_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ("1artist", model_sorted_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ("2artist", model_sorted_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ("Z", model_sorted_->index(4, 0, QModelIndex()).data().toString());
  EXPECT_EQ("zartist", model_sorted_->index(5, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, MixedCaseHeaders) {

  AddSong("Title", "Artist", "Album", 123);
  AddSong("Title", "artist", "Album", 123);
  model_->Init(false);

  ASSERT_EQ(3, model_sorted_->rowCount(QModelIndex()));
  EXPECT_EQ("A", model_sorted_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ("Artist", model_sorted_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ("artist", model_sorted_->index(2, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, UnknownArtists) {

  AddSong("Title", "", "Album", 123);
  model_->Init(false);
  model_->fetchMore(model_->index(0, 0));

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));
  QModelIndex unknown_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ("Unknown", unknown_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(unknown_index));
  EXPECT_EQ("Album", model_->index(0, 0, unknown_index).data().toString());

}

TEST_F(CollectionModelTest, UnknownAlbums) {

  AddSong("Title", "Artist", "", 123);
  AddSong("Title", "Artist", "Album", 123);
  model_->Init(false);
  model_->fetchMore(model_->index(0, 0));

  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  ASSERT_EQ(2, model_->rowCount(artist_index));

  QModelIndex unknown_album_index = model_->index(0, 0, artist_index);
  QModelIndex real_album_index = model_->index(1, 0, artist_index);

  EXPECT_EQ("Unknown", unknown_album_index.data().toString());
  EXPECT_EQ("Album", real_album_index.data().toString());

}

TEST_F(CollectionModelTest, VariousArtistSongs) {

  SongList songs;
  for (int i=0 ; i < 4 ; ++i) {
    QString n = QString::number(i+1);
    Song song;
    song.Init("Title " + n, "Artist " + n, "Album", 0);
    song.set_mtime(0);
    song.set_ctime(0);
    songs << song;
  }

  // Different ways of putting songs in "Various Artist".  Make sure they all work
  songs[0].set_compilation_detected(true);
  songs[1].set_compilation(true);
  songs[2].set_compilation_on(true);
  songs[3].set_compilation_detected(true); songs[3].set_artist("Various Artists");

  for (int i=0 ; i<4 ; ++i)
    AddSong(songs[i]);
  model_->Init(false);

  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  model_->fetchMore(artist_index);
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  model_->fetchMore(album_index);
  ASSERT_EQ(4, model_->rowCount(album_index));

  EXPECT_EQ("Artist 1 - Title 1", model_->index(0, 0, album_index).data().toString());
  EXPECT_EQ("Artist 2 - Title 2", model_->index(1, 0, album_index).data().toString());
  EXPECT_EQ("Artist 3 - Title 3", model_->index(2, 0, album_index).data().toString());
  EXPECT_EQ("Title 4", model_->index(3, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveSongsLazyLoaded) {

  Song one = AddSong("Title 1", "Artist", "Album", 123); one.set_id(1);
  Song two = AddSong("Title 2", "Artist", "Album", 123); two.set_id(2);
  AddSong("Title 3", "Artist", "Album", 123);
  model_->Init(false);

  // Lazy load the items
  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  model_->fetchMore(artist_index);
  ASSERT_EQ(1, model_->rowCount(artist_index));
  QModelIndex album_index = model_->index(0, 0, artist_index);
  model_->fetchMore(album_index);
  ASSERT_EQ(3, model_->rowCount(album_index));

  // Remove the first two songs
  QSignalSpy spy_preremove(model_.get(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)));
  QSignalSpy spy_remove(model_.get(), SIGNAL(rowsRemoved(QModelIndex,int,int)));
  QSignalSpy spy_reset(model_.get(), SIGNAL(modelReset()));

  backend_->DeleteSongs(SongList() << one << two);

  ASSERT_EQ(2, spy_preremove.count());
  ASSERT_EQ(2, spy_remove.count());
  ASSERT_EQ(0, spy_reset.count());

  artist_index = model_->index(0, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));
  album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(1, model_->rowCount(album_index));
  EXPECT_EQ("Title 3", model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveSongsNotLazyLoaded) {

  Song one = AddSong("Title 1", "Artist", "Album", 123); one.set_id(1);
  Song two = AddSong("Title 2", "Artist", "Album", 123); two.set_id(2);
  model_->Init(false);

  // Remove the first two songs
  QSignalSpy spy_preremove(model_.get(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)));
  QSignalSpy spy_remove(model_.get(), SIGNAL(rowsRemoved(QModelIndex,int,int)));
  QSignalSpy spy_reset(model_.get(), SIGNAL(modelReset()));

  backend_->DeleteSongs(SongList() << one << two);

  ASSERT_EQ(0, spy_preremove.count());
  ASSERT_EQ(0, spy_remove.count());
  ASSERT_EQ(1, spy_reset.count());

}

TEST_F(CollectionModelTest, RemoveEmptyAlbums) {

  Song one = AddSong("Title 1", "Artist", "Album 1", 123); one.set_id(1);
  Song two = AddSong("Title 2", "Artist", "Album 2", 123); two.set_id(2);
  Song three = AddSong("Title 3", "Artist", "Album 2", 123); three.set_id(3);
  model_->Init(false);

  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  model_->fetchMore(artist_index);
  ASSERT_EQ(2, model_->rowCount(artist_index));

  // Remove one song from each album
  backend_->DeleteSongs(SongList() << one << two);

  // Check the model
  artist_index = model_->index(0, 0, QModelIndex());
  model_->fetchMore(artist_index);
  ASSERT_EQ(1, model_->rowCount(artist_index));
  QModelIndex album_index = model_->index(0, 0, artist_index);
  model_->fetchMore(album_index);
  EXPECT_EQ("Album 2", album_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(album_index));
  EXPECT_EQ("Title 3", model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveEmptyArtists) {

  Song one = AddSong("Title", "Artist", "Album", 123); one.set_id(1);
  model_->Init(false);

  // Lazy load the items
  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  model_->fetchMore(artist_index);
  ASSERT_EQ(1, model_->rowCount(artist_index));
  QModelIndex album_index = model_->index(0, 0, artist_index);
  model_->fetchMore(album_index);
  ASSERT_EQ(1, model_->rowCount(album_index));

  // The artist header is there too right?
  ASSERT_EQ(2, model_->rowCount(QModelIndex()));

  // Remove the song
  backend_->DeleteSongs(SongList() << one);

  // Everything should be gone - even the artist header
  ASSERT_EQ(0, model_->rowCount(QModelIndex()));

}

// Test to check that the container nodes are created identical and unique all through the model with all possible collection groupings.
// model1 - Nodes are created from a complete reset done through lazy-loading.
// model2 - Initial container nodes are created in SongsDiscovered.
// model3 - All container nodes are created in SongsDiscovered.

// WARNING: This test can take up to 30 minutes to complete.

TEST_F(CollectionModelTest, TestContainerNodes) {

  SongList songs;
  int year = 1960;
  // Add some normal albums.
  for (int artist_number = 1; artist_number <= 3 ; ++artist_number) {
    Song song(Song::Source_Collection);
    song.set_artist(QString("Artist %1").arg(artist_number));
    song.set_composer(QString("Composer %1").arg(artist_number));
    song.set_performer(QString("Performer %1").arg(artist_number));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    for (int album_number = 1; album_number <= 3 ; ++album_number) {
      if (year > 2020) year = 1960;
      song.set_album(QString("Artist %1 - Album %2").arg(artist_number).arg(album_number));
      song.set_album_id(QString::number(album_number));
      song.set_year(year++);
      song.set_genre("Rock");
      for (int song_number = 1; song_number <= 5 ; ++song_number) {
        song.set_url(QUrl(QString("file:///mnt/music/Artist %1/Album %2/%3 - artist song-n-%3").arg(artist_number).arg(album_number).arg(song_number)));
        song.set_title(QString("Title %1").arg(song_number));
        song.set_track(song_number);
        songs << song;
      }
    }
  }

  // Add some albums with 'album artist'.
  for (int album_artist_number = 1; album_artist_number <= 3 ; ++album_artist_number) {
    Song song(Song::Source_Collection);
    song.set_albumartist(QString("Album Artist %1").arg(album_artist_number));
    song.set_composer(QString("Composer %1").arg(album_artist_number));
    song.set_performer(QString("Performer %1").arg(album_artist_number));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    for (int album_number = 1; album_number <= 3 ; ++album_number) {
      if (year > 2020) year = 1960;
      song.set_album(QString("Album Artist %1 - Album %2").arg(album_artist_number).arg(album_number));
      song.set_album_id(QString::number(album_number));
      song.set_year(year++);
      song.set_genre("Rock");
      int artist_number = 1;
      for (int song_number = 1; song_number <= 5 ; ++song_number) {
        song.set_url(QUrl(QString("file:///mnt/music/Album Artist %1/Album %2/%3 - album artist song-n-%3").arg(album_artist_number).arg(album_number).arg(QString::number(song_number))));
        song.set_title("Title " + QString::number(song_number));
        song.set_track(song_number);
        song.set_artist("Artist " + QString::number(artist_number));
        songs << song;
        ++artist_number;
      }
    }
  }

  // Add some compilation albums.
  for (int album_number = 1; album_number <= 3 ; ++album_number) {
    if (year > 2020) year = 1960;
    Song song(Song::Source_Collection);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_album(QString("Compilation Album %1").arg(album_number));
    song.set_album_id(QString::number(album_number));
    song.set_year(year++);
    song.set_genre("Pop");
    song.set_compilation(true);
    int artist_number = 1;
    for (int song_number = 1; song_number <= 4 ; ++song_number) {
      song.set_url(QUrl(QString("file:///mnt/music/Compilation Artist %1/Compilation Album %2/%3 - compilation song-n-%3").arg(artist_number).arg(album_number).arg(QString::number(song_number))));
      song.set_artist(QString("Compilation Artist %1").arg(artist_number));
      song.set_composer(QString("Composer %1").arg(artist_number));
      song.set_performer(QString("Performer %1").arg(artist_number));
      song.set_title(QString("Title %1").arg(song_number));
      song.set_track(song_number);
      songs << song;
      ++artist_number;
    }
  }

  // Songs with only title
  {
    Song song(Song::Source_Collection);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_url(QUrl(QString("file:///mnt/music/no album song 1/song-only-1")));
    song.set_title("Only Title 1");
    songs << song;
    song.set_url(QUrl(QString("file:///mnt/music/no album song 2/song-only-2")));
    song.set_title("Only Title 2");
    songs << song;
  }

  // Song with only artist, album and title.
  {
    Song song(Song::Source_Collection);
    song.set_url(QUrl(QString("file:///tmp/artist-album-title-song")));
    song.set_artist("Not Only Artist");
    song.set_album("Not Only Album");
    song.set_title("Not Only Title");
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  // Add possible Various artists conflicting songs.
  {
    Song song(Song::Source_Collection);
    song.set_url(QUrl(QString("file:///tmp/song-va-conflicting-1")));
    song.set_artist("Various artists");
    song.set_album("VA Album");
    song.set_title("VA Title");
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  {
    Song song(Song::Source_Collection);
    song.set_url(QUrl(QString("file:///tmp/song-va-conflicting-2")));
    song.set_artist("Various artists");
    song.set_albumartist("Various artists");
    song.set_album("VA Album");
    song.set_title("VA Title");
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  {
    Song song(Song::Source_Collection);
    song.set_url(QUrl(QString("file:///tmp/song-va-conflicting-3")));
    song.set_albumartist("Various artists");
    song.set_album("VA Album");
    song.set_title("VA Title");
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_directory_id(1);
    song.set_filetype(Song::FileType_FLAC);
    song.set_filesize(1);
    song.set_year(1970);
    song.set_track(1);
    songs << song;
  }

  for (int f = CollectionModel::GroupBy_None + 1 ; f < CollectionModel::GroupByCount ; ++f) {
    for (int s = CollectionModel::GroupBy_None ; s < CollectionModel::GroupByCount ; ++s) {
      for (int t = CollectionModel::GroupBy_None ; t < CollectionModel::GroupByCount ; ++t) {

        qLog(Debug) << "Testing collection model grouping: " << f << s << t;

        std::unique_ptr<Database> database1;
        std::unique_ptr<Database> database2;
        std::unique_ptr<Database> database3;
        std::unique_ptr<CollectionBackend> backend1;
        std::unique_ptr<CollectionBackend> backend2;
        std::unique_ptr<CollectionBackend> backend3;
        std::unique_ptr<CollectionModel> model1;
        std::unique_ptr<CollectionModel> model2;
        std::unique_ptr<CollectionModel> model3;

        database1.reset(new MemoryDatabase(nullptr));
        database2.reset(new MemoryDatabase(nullptr));
        database3.reset(new MemoryDatabase(nullptr));
        backend1.reset(new CollectionBackend);
        backend2.reset(new CollectionBackend);
        backend3.reset(new CollectionBackend);
        backend1->Init(database1.get(), Song::Source_Collection, SCollection::kSongsTable, SCollection::kDirsTable, SCollection::kSubdirsTable, SCollection::kFtsTable);
        backend2->Init(database2.get(), Song::Source_Collection, SCollection::kSongsTable, SCollection::kDirsTable, SCollection::kSubdirsTable, SCollection::kFtsTable);
        backend3->Init(database3.get(), Song::Source_Collection, SCollection::kSongsTable, SCollection::kDirsTable, SCollection::kSubdirsTable, SCollection::kFtsTable);
        model1.reset(new CollectionModel(backend1.get(), nullptr));
        model2.reset(new CollectionModel(backend2.get(), nullptr));
        model3.reset(new CollectionModel(backend3.get(), nullptr));

        backend1->AddDirectory("/mnt/music");
        backend2->AddDirectory("/mnt/music");
        backend3->AddDirectory("/mut/music");

        model1->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));
        model2->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));
        model3->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy(f), CollectionModel::GroupBy(s), CollectionModel::GroupBy(t)));

        model3->set_use_lazy_loading(false);

        QSignalSpy model1_update(model1.get(), SIGNAL(rowsInserted(QModelIndex, int, int)));
        QSignalSpy model2_update(model2.get(), SIGNAL(rowsInserted(QModelIndex, int, int)));
        QSignalSpy model3_update(model3.get(), SIGNAL(rowsInserted(QModelIndex, int, int)));

        backend1->AddOrUpdateSongs(songs);
        backend2->AddOrUpdateSongs(songs);
        backend3->AddOrUpdateSongs(songs);

        ASSERT_EQ(model1->song_nodes().count(), 0);
        ASSERT_EQ(model2->song_nodes().count(), 0);
        ASSERT_EQ(model3->song_nodes().count(), songs.count());

        model1->Init(false);

        model1->ExpandAll();
        model2->ExpandAll();
        // All nodes in model3 should be created already.

        ASSERT_EQ(model1->song_nodes().count(), songs.count());
        ASSERT_EQ(model2->song_nodes().count(), songs.count());
        ASSERT_EQ(model3->song_nodes().count(), songs.count());

        // Container nodes for all models should now be indentical.
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

        QSignalSpy database_reset_1(backend1.get(), SIGNAL(DatabaseReset()));
        QSignalSpy database_reset_2(backend2.get(), SIGNAL(DatabaseReset()));
        QSignalSpy database_reset_3(backend3.get(), SIGNAL(DatabaseReset()));

        backend1->DeleteAll();
        backend2->DeleteAll();
        backend3->DeleteAll();

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

}  // namespace
