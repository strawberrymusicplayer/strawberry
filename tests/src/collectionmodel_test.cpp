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

using namespace Qt::StringLiterals;
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

  AddSong(QStringLiteral("Title"), ""_L1, QStringLiteral("Album"), 123);

  ASSERT_EQ(1, model_->rowCount(QModelIndex()));
  QModelIndex unknown_index = model_->index(0, 0, QModelIndex());
  EXPECT_EQ(QStringLiteral("Unknown"), unknown_index.data().toString());

  ASSERT_EQ(1, model_->rowCount(unknown_index));
  EXPECT_EQ(QStringLiteral("Album"), model_->index(0, 0, unknown_index).data().toString());

}

TEST_F(CollectionModelTest, UnknownAlbums) {

  AddSong(QStringLiteral("Title"), QStringLiteral("Artist"), ""_L1, 123);
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

}  // namespace
