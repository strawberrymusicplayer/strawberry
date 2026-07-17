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

#include "test_utils.h"

#include "collection/collectionplaylistitem.h"
#include "playlist/playlist.h"
#include "playlist/songplaylistitem.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreaderreply.h"
#include "tagreader/tagreaderresult.h"
#include "mock_settingsprovider.h"
#include "mock_playlistitem.h"

#include <QtDebug>
#include <QUndoStack>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

using ::testing::Return;

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

// Declared at file scope (rather than inside the anonymous namespace below, where the TEST_F-generated fixture subclasses live) so that Playlist's "friend class PlaylistTest;" resolves to this exact class.
// C++ friendship isn't inherited, so tests that need access to Playlist's private members go through the CallXxx() helpers below rather than calling them directly from a TEST_F body.
class PlaylistTest : public ::testing::Test {
 protected:
  PlaylistTest()
      : playlist_(nullptr, nullptr, nullptr, nullptr, nullptr, 1),
        sequence_(nullptr, new DummySettingsProvider) {}

  void SetUp() override {
    playlist_.set_sequence(&sequence_);
  }

  // TagReaderClient::Instance() is a process-wide singleton: the constructor only ever assigns sInstance the first time one is constructed, and nothing ever resets sInstance back to nullptr on destruction.
  // SongPlaylistItem::Reload() (invoked by ItemReload()'s background task) reads through that singleton, so tests exercising it need one to exist - but it must be set up once for the whole test suite (via SetUpTestSuite(), run once before any PlaylistTest test) and never torn down here, in a per-test fixture: stopping its thread after an individual test would leave sInstance pointing at an object whose event loop is no longer running, and no later construction attempt would replace it (the "only assign if not already set" check would just see the stale pointer and do nothing), hanging or misbehaving every subsequent test in this binary that touches the singleton.
  static void SetUpTestSuite() {
    sTagReaderClientThread = new QThread();
    sTagReaderClient = new TagReaderClient();
    sTagReaderClient->moveToThread(sTagReaderClientThread);
    sTagReaderClientThread->start();
  }

  MockPlaylistItem *MakeMockItem(const QString &title, const QString &artist = QString(), const QString &album = QString(), int length = 123) const {
    Song metadata;
    metadata.Init(title, artist, album, length);

    MockPlaylistItem *ret = new MockPlaylistItem;
    EXPECT_CALL(*ret, OriginalMetadata()).WillRepeatedly(Return(metadata));

    return ret;
  }

  PlaylistItemPtr MakeMockItemP(const QString &title, const QString &artist = QString(), const QString &album = QString(), int length = 123) const {
    return PlaylistItemPtr(MakeMockItem(title, artist, album, length));
  }

  // Forwards to the private Playlist::ItemReloadComplete(), to let tests exercise the save-generation staleness check directly instead of via a real asynchronous write-then-reread round trip.
  void CallItemReloadComplete(const QPersistentModelIndex &idx, const Song &new_metadata, const bool metadata_edit, const PlaylistItemPtr &item, const quint64 save_generation, const Song &fallback_metadata = Song()) {
    playlist_.ItemReloadComplete(idx, new_metadata, metadata_edit, item, save_generation, fallback_metadata);
  }

  // Forwards to the private Playlist::SongSaveComplete(), to let tests exercise the write-failure path directly instead of via a real asynchronous tag write. On failure this now triggers a real (asynchronous) ItemReload(), so callers must pump the event loop (see WaitForEditingFinished()) for the result to apply.
  void CallSongSaveComplete(TagReaderReplyPtr reply, const QPersistentModelIndex &idx, const PlaylistItemPtr &item, const quint64 save_generation, const Song &pre_edit_metadata) {
    playlist_.SongSaveComplete(reply, idx, item, save_generation, pre_edit_metadata);
  }

  // Blocks until Playlist::EditingFinished fires, i.e. until an in-flight ItemReload()'s background reload has completed and ItemReloadComplete() has run.
  // Bounded by timeout_ms so a regression that stops EditingFinished from firing (or a reload that never completes) fails the test instead of hanging the whole run indefinitely, which would otherwise take down CI.
  void WaitForEditingFinished(const int timeout_ms = 5000) {
    QEventLoop loop;
    QObject::connect(&playlist_, &Playlist::EditingFinished, &loop, &QEventLoop::quit);
    bool timed_out = false;
    QTimer::singleShot(timeout_ms, &loop, [&loop, &timed_out]() {
      timed_out = true;
      loop.quit();
    });
    loop.exec();
    if (timed_out) {
      FAIL() << "Timed out after " << timeout_ms << " ms waiting for Playlist::EditingFinished";
    }
  }

  Playlist playlist_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  PlaylistSequence sequence_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  // Shared for the whole test suite - see the comment on SetUpTestSuite() above.
  static TagReaderClient *sTagReaderClient;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  static QThread *sTagReaderClientThread;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

};

TagReaderClient *PlaylistTest::sTagReaderClient = nullptr;
QThread *PlaylistTest::sTagReaderClientThread = nullptr;

namespace {

TEST_F(PlaylistTest, Basic) {
  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
}

TEST_F(PlaylistTest, InsertItems) {

  MockPlaylistItem *item = MakeMockItem(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
  PlaylistItemPtr item_ptr(item);

  // Insert the item
  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  playlist_.InsertItems(PlaylistItemPtrList() << item_ptr, -1);
  ASSERT_EQ(1, playlist_.rowCount(QModelIndex()));

  // Get the metadata
  EXPECT_EQ(u"Title"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Artist"_s, playlist_.data(playlist_.index(0, static_cast<int>(static_cast<int>(Playlist::Column::Artist)))));
  EXPECT_EQ(u"Album"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Album))));
  EXPECT_EQ(123, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Length))));

}

TEST_F(PlaylistTest, Indexes) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // Start "playing" track 1
  playlist_.set_current_row(0);
  EXPECT_EQ(0, playlist_.current_row());
  EXPECT_EQ(u"One"_s, playlist_.current_item()->EffectiveMetadata().title());
  EXPECT_EQ(-1, playlist_.previous_row());
  EXPECT_EQ(1, playlist_.next_row());

  // Stop playing
  EXPECT_EQ(0, playlist_.last_played_row());
  playlist_.set_current_row(-1);
  EXPECT_EQ(0, playlist_.last_played_row());
  EXPECT_EQ(-1, playlist_.current_row());

  // Play track 2
  playlist_.set_current_row(1);
  EXPECT_EQ(1, playlist_.current_row());
  EXPECT_EQ(u"Two"_s, playlist_.current_item()->EffectiveMetadata().title());
  EXPECT_EQ(0, playlist_.previous_row());
  EXPECT_EQ(2, playlist_.next_row());

  // Play track 3
  playlist_.set_current_row(2);
  EXPECT_EQ(2, playlist_.current_row());
  EXPECT_EQ(u"Three"_s, playlist_.current_item()->EffectiveMetadata().title());
  EXPECT_EQ(1, playlist_.previous_row());
  EXPECT_EQ(-1, playlist_.next_row());

}

TEST_F(PlaylistTest, RepeatPlaylist) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Playlist);

  playlist_.set_current_row(0);
  EXPECT_EQ(1, playlist_.next_row());

  playlist_.set_current_row(1);
  EXPECT_EQ(2, playlist_.next_row());

  playlist_.set_current_row(2);
  EXPECT_EQ(0, playlist_.next_row());

}

TEST_F(PlaylistTest, RepeatTrack) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Track);

  playlist_.set_current_row(0);
  EXPECT_EQ(0, playlist_.next_row());

}

TEST_F(PlaylistTest, RepeatAlbum) {

  playlist_.InsertItems(PlaylistItemPtrList()
      << MakeMockItemP(u"One"_s, u"Album one"_s)
      << MakeMockItemP(u"Two"_s, u"Album two"_s)
      << MakeMockItemP(u"Three"_s, u"Album one"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Album);

  playlist_.set_current_row(0);
  EXPECT_EQ(2, playlist_.next_row());

  playlist_.set_current_row(2);
  EXPECT_EQ(0, playlist_.next_row());

}

TEST_F(PlaylistTest, RemoveBeforeCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList()
      << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // Remove a row before the currently playing track
  playlist_.set_current_row(2);
  EXPECT_EQ(2, playlist_.current_row());
  playlist_.removeRow(1, QModelIndex());
  EXPECT_EQ(1, playlist_.current_row());
  EXPECT_EQ(1, playlist_.last_played_row());
  EXPECT_EQ(0, playlist_.previous_row());
  EXPECT_EQ(-1, playlist_.next_row());

}

TEST_F(PlaylistTest, RemoveAfterCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList()
      << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // Remove a row after the currently playing track
  playlist_.set_current_row(0);
  EXPECT_EQ(0, playlist_.current_row());
  playlist_.removeRow(1, QModelIndex());
  EXPECT_EQ(0, playlist_.current_row());
  EXPECT_EQ(0, playlist_.last_played_row());
  EXPECT_EQ(-1, playlist_.previous_row());
  EXPECT_EQ(1, playlist_.next_row());

  playlist_.set_current_row(1);
  EXPECT_EQ(-1, playlist_.next_row());

}

TEST_F(PlaylistTest, RemoveCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // Remove the currently playing track's row
  playlist_.set_current_row(1);
  EXPECT_EQ(1, playlist_.current_row());
  playlist_.removeRow(1, QModelIndex());
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());
  EXPECT_EQ(-1, playlist_.previous_row());
  EXPECT_EQ(0, playlist_.next_row());

}

TEST_F(PlaylistTest, InsertBeforeCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.set_current_row(1);
  EXPECT_EQ(1, playlist_.current_row());
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Four"_s), 0);
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));

  EXPECT_EQ(2, playlist_.current_row());
  EXPECT_EQ(2, playlist_.last_played_row());
  EXPECT_EQ(1, playlist_.previous_row());
  EXPECT_EQ(3, playlist_.next_row());

  EXPECT_EQ(u"Four"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"One"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

}

TEST_F(PlaylistTest, InsertAfterCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.set_current_row(1);
  EXPECT_EQ(1, playlist_.current_row());
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Four"_s), 2);
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));

  EXPECT_EQ(1, playlist_.current_row());
  EXPECT_EQ(1, playlist_.last_played_row());
  EXPECT_EQ(0, playlist_.previous_row());
  EXPECT_EQ(2, playlist_.next_row());

  EXPECT_EQ(u"Two"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Four"_s, playlist_.data(playlist_.index(2, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Three"_s, playlist_.data(playlist_.index(3, static_cast<int>(Playlist::Column::Title))));

}

TEST_F(PlaylistTest, Clear) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.set_current_row(1);
  EXPECT_EQ(1, playlist_.current_row());
  playlist_.Clear();

  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());
  EXPECT_EQ(-1, playlist_.previous_row());
  EXPECT_EQ(-1, playlist_.next_row());

}

TEST_F(PlaylistTest, UndoAdd) {

  EXPECT_FALSE(playlist_.undo_stack()->canUndo());
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Title"_s));
  EXPECT_EQ(1, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());

  playlist_.undo_stack()->undo();
  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canUndo());
  ASSERT_TRUE(playlist_.undo_stack()->canRedo());

  playlist_.undo_stack()->redo();
  EXPECT_EQ(1, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  EXPECT_TRUE(playlist_.undo_stack()->canUndo());

  EXPECT_EQ(u"Title"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));

}

TEST_F(PlaylistTest, UndoMultiAdd) {

  // Add 1 item
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s));

  // Add 2 items
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));

  // Undo adding 2 items
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  EXPECT_EQ(u"add 2 songs"_s, playlist_.undo_stack()->undoText());
  playlist_.undo_stack()->undo();

  // Undo adding 1 item
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  EXPECT_EQ(u"add 1 songs"_s, playlist_.undo_stack()->undoText());
  playlist_.undo_stack()->undo();

  EXPECT_FALSE(playlist_.undo_stack()->canUndo());

}

TEST_F(PlaylistTest, UndoRemove) {

  EXPECT_FALSE(playlist_.undo_stack()->canUndo());
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Title"_s));

  EXPECT_TRUE(playlist_.undo_stack()->canUndo());
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());

  playlist_.removeRow(0);

  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());

  playlist_.undo_stack()->undo();
  EXPECT_EQ(1, playlist_.rowCount(QModelIndex()));
  ASSERT_TRUE(playlist_.undo_stack()->canRedo());

  EXPECT_EQ(u"Title"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));

  playlist_.undo_stack()->redo();
  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  EXPECT_TRUE(playlist_.undo_stack()->canUndo());

}

TEST_F(PlaylistTest, UndoMultiRemove) {

  // Add 3 items
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // Remove 1 item
  playlist_.removeRow(1); // Item "Two"

  // Remove 2 items
  playlist_.removeRows(0, 2); // "One" and "Three"

  ASSERT_EQ(0, playlist_.rowCount(QModelIndex()));

  // Undo removing all 3 items
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  EXPECT_EQ(u"remove 3 songs"_s, playlist_.undo_stack()->undoText());

  playlist_.undo_stack()->undo();
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

}

TEST_F(PlaylistTest, UndoClear) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.Clear();
  ASSERT_EQ(0, playlist_.rowCount(QModelIndex()));
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  EXPECT_EQ(u"remove 3 songs"_s, playlist_.undo_stack()->undoText());
  playlist_.undo_stack()->undo();

  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

}

TEST_F(PlaylistTest, UndoRemoveCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Title"_s));
  playlist_.set_current_row(0);
  EXPECT_EQ(0, playlist_.current_row());
  EXPECT_EQ(0, playlist_.last_played_row());

  playlist_.removeRow(0);
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());

  playlist_.undo_stack()->undo();
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());

}

TEST_F(PlaylistTest, UndoRemoveOldCurrent) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Title"_s));
  playlist_.set_current_row(0);
  EXPECT_EQ(0, playlist_.current_row());
  EXPECT_EQ(0, playlist_.last_played_row());

  playlist_.removeRow(0);
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());

  playlist_.set_current_row(-1);

  playlist_.undo_stack()->undo();
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(-1, playlist_.last_played_row());

}

TEST_F(PlaylistTest, ShuffleThenNext) {

  // Add 100 items
  PlaylistItemPtrList items;
  items.reserve(100);
  for (int i = 0; i < 100; ++i)
    items << MakeMockItemP(u"Item "_s + QString::number(i));
  playlist_.InsertItems(items);

  playlist_.set_current_row(0);

  // Shuffle until the current index is not at the end
  Q_FOREVER {
    playlist_.Shuffle();
    if (playlist_.current_row() != items.count() - 1)
      break;
  }

  int index = playlist_.current_row();
  EXPECT_EQ(u"Item 0"_s, playlist_.current_item()->EffectiveMetadata().title());
  EXPECT_EQ(u"Item 0"_s, playlist_.data(playlist_.index(index, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(index, playlist_.last_played_row());
  // EXPECT_EQ(index + 1, playlist_.next_row());

  // Shuffle until the current index *is* at the end
  // forever {
  // playlist_.Shuffle();
  // if (playlist_.current_row() == items.count()-1)
  // break;
  // }

  index = playlist_.current_row();
  EXPECT_EQ(u"Item 0"_s, playlist_.current_item()->EffectiveMetadata().title());
  EXPECT_EQ(u"Item 0"_s, playlist_.data(playlist_.index(index, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(index, playlist_.last_played_row());
  // EXPECT_EQ(-1, playlist_.next_row());
  // EXPECT_EQ(index-1, playlist_.previous_row());

}

TEST_F(PlaylistTest, CollectionIdMapSingle) {

  Song song(Song::Source::Collection);
  song.Init(u"title"_s, u"artist"_s, u"album"_s, 123);
  song.set_id(1);

  PlaylistItemPtr item(std::make_shared<CollectionPlaylistItem>(song));
  playlist_.InsertItems(PlaylistItemPtrList() << item);

  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, -1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 0).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 2).count());
  ASSERT_EQ(1, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(song.title(), playlist_.collection_items(Song::Source::Collection, 1)[0]->EffectiveMetadata().title());  // clazy:exclude=detaching-temporary

  playlist_.Clear();

  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 1).count());

}

TEST_F(PlaylistTest, CollectionIdMapInvalid) {

  Song invalid;
  invalid.Init(u"title"_s, u"artist"_s, u"album"_s, 123);
  ASSERT_EQ(-1, invalid.id());

  PlaylistItemPtr item(std::make_shared<CollectionPlaylistItem>(invalid));
  playlist_.InsertItems(PlaylistItemPtrList() << item);

  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, -1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 0).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 2).count());

}

TEST_F(PlaylistTest, CollectionIdMapMulti) {

  Song one(Song::Source::Collection);
  one.Init(u"title"_s, u"artist"_s, u"album"_s, 123);
  one.set_id(1);

  Song two(Song::Source::Collection);
  two.Init(u"title 2"_s, u"artist 2"_s, u"album 2"_s, 123);
  two.set_id(2);

  PlaylistItemPtr item_one(std::make_shared<CollectionPlaylistItem>(one));
  PlaylistItemPtr item_two(std::make_shared<CollectionPlaylistItem>(two));
  PlaylistItemPtr item_three(std::make_shared<CollectionPlaylistItem>(one));
  playlist_.InsertItems(PlaylistItemPtrList() << item_one << item_two << item_three);

  EXPECT_EQ(2, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(1, playlist_.collection_items(Song::Source::Collection, 2).count());

  playlist_.removeRow(1); // item_two
  EXPECT_EQ(2, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 2).count());

  playlist_.removeRow(1); // item_three
  EXPECT_EQ(1, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 2).count());

  playlist_.removeRow(0); // item_one
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 1).count());
  EXPECT_EQ(0, playlist_.collection_items(Song::Source::Collection, 2).count());

}


TEST_F(PlaylistTest, PreviousRowIsNonMutating) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.set_current_row(0);
  playlist_.set_current_row(1);
  playlist_.set_current_row(2);

  // previous_row() should return the same row each time without consuming history
  EXPECT_EQ(1, playlist_.previous_row());
  EXPECT_EQ(1, playlist_.previous_row());
  EXPECT_EQ(1, playlist_.previous_row());

}

TEST_F(PlaylistTest, TakePreviousRowConsumesHistory) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  playlist_.set_current_row(0);
  playlist_.set_current_row(1);
  playlist_.set_current_row(2);

  // take_previous_row() should return the same row as previous_row() before consuming
  const int previous_row = playlist_.previous_row();
  const int take_previous_row = playlist_.take_previous_row(false);
  EXPECT_EQ(previous_row, take_previous_row);

  // After consuming row 1, previous_row() and take_previous_row() should now see row 0 as the previous
  EXPECT_EQ(0, playlist_.previous_row());
  EXPECT_EQ(0, playlist_.take_previous_row(false));

  // History is now empty; should fall back to sequence-based previous (row 1, the item before row 2)
  EXPECT_EQ(1, playlist_.take_previous_row(false));

  // Verify fallback is stable: repeated calls without new history still return the sequence-based previous
  EXPECT_EQ(1, playlist_.take_previous_row(false));

}

// Regression test for a race between two consecutive inline edits to the same playlist item: if the first edit's write-then-reread round trip completes after the second edit's, it must not clobber the second (newer) edit with its own stale result.
TEST_F(PlaylistTest, StaleReloadCompletionDoesNotClobberNewerEdit) {

  Song song;
  song.Init(u"Title"_s, u"OriginalArtist"_s, u"Album"_s, 123);
  song.set_url(QUrl::fromLocalFile(u"/tmp/does-not-need-to-exist.mp3"_s));

  PlaylistItemPtr item = std::make_shared<SongPlaylistItem>(song, false);
  playlist_.InsertItems(PlaylistItemPtrList() << item, -1);
  const QPersistentModelIndex idx(playlist_.index(0, static_cast<int>(Playlist::Column::Artist)));

  ASSERT_EQ(u"OriginalArtist"_s, item->OriginalMetadata().artist());

  // Simulate the user making two consecutive edits to the same cell before the first edit's async write-then-reread round trip (see Playlist::setData()) has completed: each edit bumps the item's save generation, exactly as setData() does.
  const quint64 generation_edit_one = item->BumpSaveGeneration();
  const quint64 generation_edit_two = item->BumpSaveGeneration();
  ASSERT_NE(generation_edit_one, generation_edit_two);

  Song metadata_edit_one = item->OriginalMetadata();
  metadata_edit_one.set_artist(u"EditOneArtist"_s);

  Song metadata_edit_two = item->OriginalMetadata();
  metadata_edit_two.set_artist(u"EditTwoArtist"_s);

  // The second (newer) edit's reload completes first.
  CallItemReloadComplete(idx, metadata_edit_two, true, item, generation_edit_two);
  EXPECT_EQ(u"EditTwoArtist"_s, item->OriginalMetadata().artist());

  // The first edit's reload, started earlier but slower, completes afterwards. Its captured generation no longer matches the item's current generation, so this stale completion must be discarded rather than clobbering the newer edit.
  CallItemReloadComplete(idx, metadata_edit_one, true, item, generation_edit_one);
  EXPECT_EQ(u"EditTwoArtist"_s, item->OriginalMetadata().artist());

}

// Regression test: if the tag write itself fails and the subsequent reload also can't read the file (here because it doesn't exist), the optimistic value shown by setData() must fall back to the pre-edit value instead of being left displayed (and later persisted) despite never having been written.
TEST_F(PlaylistTest, FailedSaveAndFailedReloadFallsBackToPreEditMetadata) {

  Song song;
  song.Init(u"Title"_s, u"OriginalArtist"_s, u"Album"_s, 123);
  QTemporaryFile missing_file;
  ASSERT_TRUE(missing_file.open());
  const QString missing_path = missing_file.fileName();
  missing_file.close();
  ASSERT_TRUE(missing_file.remove());
  song.set_url(QUrl::fromLocalFile(missing_path));
  PlaylistItemPtr item = std::make_shared<SongPlaylistItem>(song, false);
  playlist_.InsertItems(PlaylistItemPtrList() << item, -1);
  const QPersistentModelIndex idx(playlist_.index(0, static_cast<int>(Playlist::Column::Artist)));

  const Song pre_edit_metadata = item->OriginalMetadata();

  // Simulate setData()'s optimistic update: bump the save generation and apply the edited value immediately, exactly as setData() does before the async write starts.
  const quint64 save_generation = item->BumpSaveGeneration();
  Song edited_metadata = pre_edit_metadata;
  edited_metadata.set_artist(u"EditedArtist"_s);
  playlist_.UpdateItemMetadata(0, item, edited_metadata, false);
  ASSERT_EQ(u"EditedArtist"_s, item->OriginalMetadata().artist());

  // The write fails, and since the file doesn't exist, the reload SongSaveComplete() triggers to resync with disk will fail too.
  TagReaderReplyPtr reply(new TagReaderReply(song.url().toLocalFile()));
  reply->set_result(TagReaderResult(TagReaderResult::ErrorCode::FileSaveError));

  CallSongSaveComplete(reply, idx, item, save_generation, pre_edit_metadata);
  WaitForEditingFinished();

  // With no genuine disk state to fall back on, the pre-edit value is the best available: the optimistic edit must not be left displayed (and persisted) despite never having been written to disk.
  EXPECT_EQ(u"OriginalArtist"_s, item->OriginalMetadata().artist());

}

// Regression test: for consecutive edits to the same item, the metadata restored after a failed write must reflect the actual on-disk state, not just whatever the previous (possibly also-unwritten) optimistic edit happened to leave in place.
TEST_F(PlaylistTest, FailedSaveReloadsActualDiskStateRatherThanStalePreEditChain) {

  TemporaryResource resource(u":/audio/strawberry.mp3"_s);

  // Establish a known baseline actually on disk.
  Song baseline;
  baseline.Init(u"Title"_s, u"RealDiskArtist"_s, u"Album"_s, 123);
  baseline.set_url(QUrl::fromLocalFile(resource.fileName()));
  {
    TagReaderReplyPtr write_reply = sTagReaderClient->WriteFileAsync(resource.fileName(), baseline);
    QEventLoop loop;
    QObject::connect(&*write_reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    ASSERT_TRUE(write_reply->success());
  }

  PlaylistItemPtr item = std::make_shared<SongPlaylistItem>(baseline, false);
  playlist_.InsertItems(PlaylistItemPtrList() << item, -1);
  const QPersistentModelIndex idx(playlist_.index(0, static_cast<int>(Playlist::Column::Artist)));

  // Edit 1: optimistically applied, its write is still (conceptually) in flight. Its generation isn't needed here since edit 1's own (still in-flight) completion is never delivered in this test.
  item->BumpSaveGeneration();
  Song metadata_edit_one = item->OriginalMetadata();
  metadata_edit_one.set_artist(u"EditOneArtist"_s);
  playlist_.UpdateItemMetadata(0, item, metadata_edit_one, false);

  // Edit 2 follows before edit 1's write completes: its pre-edit value is edit 1's optimistic (unconfirmed) artist, not what is genuinely on disk.
  const Song pre_edit_metadata_two = item->OriginalMetadata();
  ASSERT_EQ(u"EditOneArtist"_s, pre_edit_metadata_two.artist());
  const quint64 generation_edit_two = item->BumpSaveGeneration();
  Song metadata_edit_two = pre_edit_metadata_two;
  metadata_edit_two.set_artist(u"EditTwoArtist"_s);
  playlist_.UpdateItemMetadata(0, item, metadata_edit_two, false);

  // Edit 2's write fails.
  TagReaderReplyPtr reply(new TagReaderReply(resource.fileName()));
  reply->set_result(TagReaderResult(TagReaderResult::ErrorCode::FileSaveError));

  CallSongSaveComplete(reply, idx, item, generation_edit_two, pre_edit_metadata_two);
  WaitForEditingFinished();

  // The item must reflect what is genuinely on disk ("RealDiskArtist"), not edit 1's stale, never-confirmed optimistic value ("EditOneArtist") that pre_edit_metadata_two happened to carry.
  EXPECT_EQ(u"RealDiskArtist"_s, item->OriginalMetadata().artist());

}

}  // namespace
