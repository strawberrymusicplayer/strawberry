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

#include <QMap>
#include <QString>
#include <QUrl>
#include <QThread>
#include <QSignalSpy>
#include <QtDebug>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/memorydatabase.h"
#include "collection/collectionlibrary.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"

using namespace Qt::Literals::StringLiterals;
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
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionLibrary::kSongsTable), QLatin1String(CollectionLibrary::kDirsTable), QLatin1String(CollectionLibrary::kSubdirsTable));
    model_ = make_unique<CollectionModel>(backend_, nullptr);
    collection_filter_ = model_->filter();

    added_dir_ = false;

  }

  Song AddSong(Song &song) {
    song.set_directory_id(1);
    if (song.mtime() == 0) song.set_mtime(1);
    if (song.ctime() == 0) song.set_ctime(1);
    if (song.url().isEmpty()) song.set_url(QUrl(u"file:///tmp/foo"_s));
    if (song.filesize() == -1) song.set_filesize(1);

    if (!added_dir_) {
      backend_->AddDirectory(u"/tmp"_s);
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

  AddSong(u"Title"_s, u"Artist 1"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"Artist 2"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"Foo"_s, u"Album"_s, 123);

  ASSERT_EQ(5, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"Artist 1"_s, collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"Artist 2"_s, collection_filter_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"F"_s, collection_filter_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"Foo"_s, collection_filter_->index(4, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, CompilationAlbums) {

  Song song;
  song.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
  song.set_compilation(true);
  song.set_mtime(0);
  song.set_ctime(0);

  AddSong(song);

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));

  QModelIndex va_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ(u"Various artists"_s, va_index.data().toString());
  EXPECT_TRUE(model_->hasChildren(va_index));
  ASSERT_EQ(model_->rowCount(va_index), 1);

  QModelIndex album_index = model_->index(0, 0, va_index);
  EXPECT_EQ(model_->data(album_index).toString(), u"Album"_s);
  EXPECT_TRUE(model_->hasChildren(album_index));

}

TEST_F(CollectionModelTest, NumericHeaders) {

  AddSong(u"Title"_s, u"1artist"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"2artist"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"0artist"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"zartist"_s, u"Album"_s, 123);

  ASSERT_EQ(6, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(u"0-9"_s, collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"0artist"_s, collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"1artist"_s, collection_filter_->index(2, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"2artist"_s, collection_filter_->index(3, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"Z"_s, collection_filter_->index(4, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"zartist"_s, collection_filter_->index(5, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, MixedCaseHeaders) {

  AddSong(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
  AddSong(u"Title"_s, u"artist"_s, u"Album"_s, 123);

  ASSERT_EQ(3, collection_filter_->rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, collection_filter_->index(0, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"Artist"_s, collection_filter_->index(1, 0, QModelIndex()).data().toString());
  EXPECT_EQ(u"artist"_s, collection_filter_->index(2, 0, QModelIndex()).data().toString());

}

TEST_F(CollectionModelTest, UnknownArtists) {

  AddSong(u"Title"_s, ""_L1, u"Album"_s, 123);

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));
  QModelIndex unknown_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ(u"Unknown"_s, unknown_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(unknown_index));
  EXPECT_EQ(u"Album"_s, model_->index(0, 0, unknown_index).data().toString());

}

TEST_F(CollectionModelTest, UnknownAlbums) {

  AddSong(u"Title"_s, u"Artist"_s, ""_L1, 123);
  AddSong(u"Title"_s, u"Artist"_s, u"Album"_s, 123);

  QModelIndex artist_index = model_->index(1, 0, QModelIndex());
  EXPECT_EQ(artist_index.isValid(), true);
  ASSERT_EQ(2, model_->rowCount(artist_index));

  QModelIndex unknown_album_index = model_->index(0, 0, artist_index);
  QModelIndex real_album_index = model_->index(1, 0, artist_index);

  EXPECT_EQ(u"Unknown"_s, unknown_album_index.data().toString());
  EXPECT_EQ(u"Album"_s, real_album_index.data().toString());

}

TEST_F(CollectionModelTest, VariousArtistSongs) {

  SongList songs;
  for (int i=0 ; i < 4 ; ++i) {
    QString n = QString::number(i+1);
    Song song;
    song.Init(u"Title "_s + n, u"Artist "_s + n, u"Album"_s, 0);
    song.set_mtime(0);
    song.set_ctime(0);
    songs << song;  // clazy:exclude=reserve-candidates
  }

  // Different ways of putting songs in "Various Artist".  Make sure they all work
  songs[0].set_compilation_detected(true);
  songs[1].set_compilation(true);
  songs[2].set_compilation_on(true);
  songs[3].set_compilation_detected(true); songs[3].set_artist(u"Various Artists"_s);

  for (int i=0 ; i < 4 ; ++i)
    AddSong(songs[i]);

  QModelIndex artist_index = model_->index(0, 0, QModelIndex());
  ASSERT_EQ(1, model_->rowCount(artist_index));

  QModelIndex album_index = model_->index(0, 0, artist_index);
  ASSERT_EQ(4, model_->rowCount(album_index));

  EXPECT_EQ(u"Artist 1 - Title 1"_s, model_->index(0, 0, album_index).data().toString());
  EXPECT_EQ(u"Artist 2 - Title 2"_s, model_->index(1, 0, album_index).data().toString());
  EXPECT_EQ(u"Artist 3 - Title 3"_s, model_->index(2, 0, album_index).data().toString());
  EXPECT_EQ(u"Title 4"_s, model_->index(3, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveSongs) {

  Song one = AddSong(u"Title 1"_s, u"Artist"_s, u"Album"_s, 123); one.set_id(1);
  Song two = AddSong(u"Title 2"_s, u"Artist"_s, u"Album"_s, 123); two.set_id(2);
  AddSong(u"Title 3"_s, u"Artist"_s, u"Album"_s, 123);

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
  EXPECT_EQ(u"Title 3"_s, model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveEmptyAlbums) {

  Song one = AddSong(u"Title 1"_s, u"Artist"_s, u"Album 1"_s, 123); one.set_id(1);
  Song two = AddSong(u"Title 2"_s, u"Artist"_s, u"Album 2"_s, 123); two.set_id(2);
  Song three = AddSong(u"Title 3"_s, u"Artist"_s, u"Album 2"_s, 123); three.set_id(3);

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
  EXPECT_EQ(u"Album 2"_s, album_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(album_index));
  EXPECT_EQ(u"Title 3"_s, model_->index(0, 0, album_index).data().toString());

}

TEST_F(CollectionModelTest, RemoveEmptyArtists) {

  Song one = AddSong(u"Title"_s, u"Artist"_s, u"Album"_s, 123); one.set_id(1);

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

}  // namespace
