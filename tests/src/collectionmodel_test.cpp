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

#include <algorithm>
#include <memory>

#include "gtest_include.h"

#include <QMap>
#include <QString>
#include <QUrl>
#include <QThread>
#include <QSignalSpy>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QPersistentModelIndex>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QTreeView>
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

  // Build n distinct songs spread over a few artists/albums, all tagged so different batches don't collide.
  SongList MakeSongs(const int n, const QString &tag) {
    SongList songs;
    songs.reserve(n);
    for (int i = 0; i < n; ++i) {
      const QString num = QString::number(i);
      const QString artist = tag + u"_artist_"_s + QString::number(i % 50);
      Song song;
      song.Init(tag + u"_title_"_s + num, artist, tag + u"_album_"_s + QString::number(i % 200), 123);
      song.set_albumartist(artist);
      song.set_directory_id(1);
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_url(QUrl(u"file:///tmp/"_s + tag + u'_' + num + u".flac"_s));
      song.set_filesize(1);
      songs << song;  // clazy:exclude=reserve-candidates
    }
    return songs;
  }

  CollectionItem *ItemForSongId(const int id) {
    const QList<CollectionItem*> nodes = model_->song_nodes();
    for (CollectionItem *node : nodes) {
      if (node->metadata.id() == id) return node;
    }
    return nullptr;
  }

  // Pump the event loop until the model's update timer has been continuously idle for a stretch.
  // The idle window must outlast the gap a Reset's async SQL reload leaves before it re-arms the timer, or we'd stop mid-reload.
  void Drain(const int max_ms = 60000) {
    QTimer *timer = model_->findChild<QTimer*>(QString(), Qt::FindDirectChildrenOnly);
    QElapsedTimer total;
    total.start();
    QElapsedTimer idle;
    idle.start();
    while (total.elapsed() < max_ms) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
      if (timer && timer->isActive()) {
        idle.restart();
        continue;
      }
      if (idle.elapsed() >= 250) break;
    }
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
  for (int i = 0 ; i < 4 ; ++i) {
    QString n = QString::number(i + 1);
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

  for (int i = 0 ; i < 4 ; ++i)
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

  // Title 1 and Title 2 occupy contiguous rows 0 and 1, so RemoveSiblingNodes collapses them into a single beginRemoveRows/endRemoveRows pair.
  ASSERT_EQ(1, spy_preremove.count());
  ASSERT_EQ(1, spy_remove.count());
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

// A batch at/above the bulk threshold collapses into a layout change and emits neither per-row insert signals nor a model reset, while still producing the correct tree.
TEST_F(CollectionModelTest, BulkUpdateSuppressesPerRowSignals) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);  // drains the ctor reset, adds the dir

  const int n = 2000;  // >= kBulkUpdateSongThreshold
  const SongList songs = MakeSongs(n, u"bulk"_s);

  QSignalSpy spy_insert(&*model_, &CollectionModel::rowsInserted);
  QSignalSpy spy_layout(&*model_, &CollectionModel::layoutChanged);
  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  backend_->AddOrUpdateSongs(songs);
  Drain();

  EXPECT_EQ(0, spy_insert.count());
  EXPECT_GE(spy_layout.count(), 1);
  EXPECT_EQ(0, spy_reset.count());
  EXPECT_EQ(n + 1, static_cast<int>(model_->song_nodes().count()));

}

// A batch below the threshold keeps the lightweight per-row path (no layout change, no reset).
TEST_F(CollectionModelTest, SmallUpdateEmitsPerRowSignals) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);

  const int n = 10;  // < kBulkUpdateSongThreshold
  const SongList songs = MakeSongs(n, u"small"_s);

  QSignalSpy spy_insert(&*model_, &CollectionModel::rowsInserted);
  QSignalSpy spy_layout(&*model_, &CollectionModel::layoutChanged);
  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  backend_->AddOrUpdateSongs(songs);
  Drain();

  EXPECT_GT(spy_insert.count(), 0);
  EXPECT_EQ(0, spy_layout.count());
  EXPECT_EQ(0, spy_reset.count());
  EXPECT_EQ(n + 1, static_cast<int>(model_->song_nodes().count()));

}

// Regression for the empty/double transaction: a bulk batch must apply its changes inside a single layout-change transaction, not re-queue them into a second one.
TEST_F(CollectionModelTest, LargeBatchProducesSingleLayoutChange) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);

  const SongList songs = MakeSongs(2000, u"once"_s);

  QSignalSpy spy_layout(&*model_, &CollectionModel::layoutChanged);
  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  backend_->AddOrUpdateSongs(songs);
  Drain();

  EXPECT_EQ(1, spy_layout.count());
  EXPECT_EQ(0, spy_reset.count());
  EXPECT_EQ(2001, static_cast<int>(model_->song_nodes().count()));

}

// A Reset queued behind a bulk-sized batch must be handled outside the bulk transaction:
// folding it in would tear the model down mid-layout-change and leave the begin/end signals of either transaction unbalanced.
// NOTE: data preservation across the Reset cannot be checked here - the reload runs on a worker thread, which opens its own connection to the :memory: test DB and so sees it empty;
// against a real file DB that reload restores the full collection, which is why a dropped trailing update is not actually lost in production.
TEST_F(CollectionModelTest, ResetMixedIntoBulkBatchDoesNotNestResets) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);

  QSignalSpy spy_about(&*model_, &CollectionModel::modelAboutToBeReset);
  QSignalSpy spy_done(&*model_, &CollectionModel::modelReset);
  QSignalSpy spy_layout_about(&*model_, &CollectionModel::layoutAboutToBeChanged);
  QSignalSpy spy_layout_done(&*model_, &CollectionModel::layoutChanged);

  // Backend signals are emitted synchronously, so these stack up in the update queue as [AddReAddOrUpdate(a)..., Reset, AddReAddOrUpdate(b)] before the timer-driven ProcessUpdate ever runs.
  backend_->AddOrUpdateSongs(MakeSongs(1600, u"aaa"_s));
  model_->SetGroupBy(model_->GetGroupBy());  // enqueues a Reset behind the batch
  backend_->AddOrUpdateSongs(MakeSongs(50, u"bbb"_s));

  Drain();

  EXPECT_EQ(spy_about.count(), spy_done.count());
  EXPECT_GE(spy_done.count(), 1);
  EXPECT_EQ(spy_layout_about.count(), spy_layout_done.count());

}

// The bulk path must not discard view state: persistent indexes (what a view uses for selection, scroll position and expanded containers) survive the layout change.
// Remapping is identity-based, so a song that is removed and re-added under a new container - the streaming-metadata case - keeps its persistent index too;
// only genuinely removed items go invalid.
TEST_F(CollectionModelTest, BulkUpdatePreservesPersistentIndexes) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);

  backend_->AddOrUpdateSongs(MakeSongs(2000, u"keep"_s));
  Drain();

  // Pull the stored songs (now carrying their database ids) back out of the model, in a stable order.
  SongList stored;
  const QList<CollectionItem*> nodes = model_->song_nodes();
  for (CollectionItem *node : nodes) {
    if (node->metadata.title().startsWith(u"keep_"_s)) stored << node->metadata;  // clazy:exclude=reserve-candidates
  }
  ASSERT_EQ(2000, stored.count());
  std::sort(stored.begin(), stored.end(), [](const Song &a, const Song &b) { return a.id() < b.id(); });

  const int updated_id = stored[10].id();  // metadata-only change, node mutated in place
  const int readded_id = stored[11].id();  // album change moves it to a new container
  const int removed_id = stored[12].id();  // deleted entirely

  CollectionItem *updated_item = ItemForSongId(updated_id);
  CollectionItem *readded_item = ItemForSongId(readded_id);
  CollectionItem *removed_item = ItemForSongId(removed_id);
  ASSERT_TRUE(updated_item && readded_item && removed_item);

  const QPersistentModelIndex p_updated(model_->ItemToIndex(updated_item));
  const QPersistentModelIndex p_readded(model_->ItemToIndex(readded_item));
  const QPersistentModelIndex p_removed(model_->ItemToIndex(removed_item));
  const QPersistentModelIndex p_container(model_->ItemToIndex(updated_item).parent());  // the album node
  ASSERT_TRUE(p_updated.isValid() && p_readded.isValid() && p_removed.isValid() && p_container.isValid());

  // Touch every song's title so the whole batch goes through the bulk path, and queue one removal behind it; both drain in the same transaction.
  SongList changed = stored;
  changed.removeAt(12);
  for (int i = 0; i < changed.count(); ++i) {
    changed[i].set_title(changed[i].title() + u"_v2"_s);
  }
  changed[11].set_album(u"keep_album_moved"_s);

  QSignalSpy spy_layout(&*model_, &CollectionModel::layoutChanged);
  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  backend_->AddOrUpdateSongs(changed);
  backend_->DeleteSongs(SongList() << stored[12]);
  Drain();

  EXPECT_GE(spy_layout.count(), 1);
  EXPECT_EQ(0, spy_reset.count());
  EXPECT_EQ(2000, static_cast<int>(model_->song_nodes().count()));  // 2000 keep + seed - 1 removed

  // Updated in place: still valid and pointing at the same song.
  CollectionItem *updated_item_after = ItemForSongId(updated_id);
  ASSERT_TRUE(updated_item_after);
  EXPECT_TRUE(p_updated.isValid());
  EXPECT_EQ(model_->ItemToIndex(updated_item_after), QModelIndex(p_updated));
  EXPECT_EQ(stored[10].title() + u"_v2"_s, updated_item_after->metadata.title());

  // Removed and re-added under a new container: the remap follows the new node.
  CollectionItem *readded_item_after = ItemForSongId(readded_id);
  ASSERT_TRUE(readded_item_after);
  EXPECT_TRUE(p_readded.isValid());
  EXPECT_EQ(model_->ItemToIndex(readded_item_after), QModelIndex(p_readded));
  EXPECT_EQ(u"keep_album_moved"_s, p_readded.parent().data().toString());

  // Genuinely removed: invalidated instead of left dangling.
  EXPECT_FALSE(p_removed.isValid());

  // The surviving album container keeps its persistent index too.
  EXPECT_TRUE(p_container.isValid());
  EXPECT_EQ(p_container, p_updated.parent());

}

// End-to-end check of what the bulk path actually promises:
// a real QTreeView attached through the CollectionFilter proxy keeps its expanded containers, its selection and its scroll position across a bulk update -
// including a selected song that is removed and re-added under a new container. With the old model-reset transaction all three were discarded.
TEST_F(CollectionModelTest, BulkUpdatePreservesViewState) {

  AddSong(u"seed"_s, u"seed"_s, u"seed"_s, 1);
  backend_->AddOrUpdateSongs(MakeSongs(2000, u"keep"_s));
  Drain();

  QTreeView view;
  view.setSelectionMode(QAbstractItemView::ExtendedSelection);
  view.setModel(collection_filter_);
  view.resize(400, 300);
  view.show();
  QCoreApplication::processEvents();

  SongList stored;
  const QList<CollectionItem*> nodes = model_->song_nodes();
  for (CollectionItem *node : nodes) {
    if (node->metadata.title().startsWith(u"keep_"_s)) stored << node->metadata;  // clazy:exclude=reserve-candidates
  }
  ASSERT_EQ(2000, stored.count());
  std::sort(stored.begin(), stored.end(), [](const Song &a, const Song &b) { return a.id() < b.id(); });

  const int updated_id = stored[10].id();  // metadata-only change
  const int readded_id = stored[11].id();  // album change moves it to a new container

  const auto proxy_index_for_id = [this](const int id) {
    CollectionItem *item = ItemForSongId(id);
    return item ? collection_filter_->mapFromSource(model_->ItemToIndex(item)) : QModelIndex();
  };

  const QModelIndex updated_proxy = proxy_index_for_id(updated_id);
  const QModelIndex readded_proxy = proxy_index_for_id(readded_id);
  ASSERT_TRUE(updated_proxy.isValid());
  ASSERT_TRUE(readded_proxy.isValid());

  // Expand the artist and album above the updated song, select both songs, and scroll away from the top.
  view.expand(updated_proxy.parent().parent());
  view.expand(updated_proxy.parent());
  ASSERT_TRUE(view.isExpanded(updated_proxy.parent()));
  view.selectionModel()->select(updated_proxy, QItemSelectionModel::Select);
  view.selectionModel()->select(readded_proxy, QItemSelectionModel::Select);
  ASSERT_EQ(2, view.selectionModel()->selectedIndexes().count());
  view.scrollToBottom();
  QCoreApplication::processEvents();
  ASSERT_GT(view.verticalScrollBar()->value(), 0);

  // Bulk update: every title changes (in place), one selected song moves to a new album (remove + re-add).
  SongList changed = stored;
  for (int i = 0; i < changed.count(); ++i) {
    changed[i].set_title(changed[i].title() + u"_v2"_s);
  }
  changed[11].set_album(u"keep_album_moved"_s);

  QSignalSpy spy_reset(&*model_, &CollectionModel::modelReset);

  backend_->AddOrUpdateSongs(changed);
  Drain();

  ASSERT_EQ(0, spy_reset.count());  // went through the bulk layout-change path

  // Expansion survived.
  CollectionItem *updated_item = ItemForSongId(updated_id);
  ASSERT_TRUE(updated_item);
  const QModelIndex album_proxy = collection_filter_->mapFromSource(model_->ItemToIndex(updated_item->parent));
  const QModelIndex artist_proxy = album_proxy.parent();
  ASSERT_TRUE(album_proxy.isValid());
  EXPECT_TRUE(view.isExpanded(album_proxy));
  EXPECT_TRUE(view.isExpanded(artist_proxy));

  // Selection survived and still points at the right songs, including the one that now lives under a different album.
  const QModelIndexList selected = view.selectionModel()->selectedIndexes();
  ASSERT_EQ(2, selected.count());
  QSet<int> selected_ids;
  for (const QModelIndex &idx : selected) {
    selected_ids << model_->IndexToItem(collection_filter_->mapToSource(idx))->metadata.id();
  }
  EXPECT_TRUE(selected_ids.contains(updated_id));
  EXPECT_TRUE(selected_ids.contains(readded_id));

  // The view was not thrown back to the top (a model reset would do that).
  EXPECT_GT(view.verticalScrollBar()->value(), 0);

}

}  // namespace
