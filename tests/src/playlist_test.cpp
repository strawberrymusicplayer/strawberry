/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <functional>
#include <memory>

#include "gtest_include.h"

#include "test_utils.h"

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/taskmanager.h"
#include "collection/collectionplaylistitem.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/songplaylistitem.h"
#include "queue/queue.h"
#include "smartplaylists/playlistgenerator.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreaderreply.h"
#include "tagreader/tagreaderresult.h"
#include "mock_settingsprovider.h"
#include "mock_playlistitem.h"

#include <QtDebug>
#include <QMimeData>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QSet>
#include <QSignalSpy>
#include <QStringList>
#include <QPersistentModelIndex>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QEvent>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QAtomicInt>

using ::testing::Return;

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

// A dynamic playlist generator which blocks generating more items until it is released, so a test can advance the playlist while items are being generated.
class BlockingDynamicGenerator : public PlaylistGenerator {
 public:
  explicit BlockingDynamicGenerator(const int future, const bool block_generate = false, const bool generate_more_empty = false, QObject *parent = nullptr) : PlaylistGenerator(parent), future_(future), block_generate_(block_generate), generate_more_empty_(generate_more_empty) {}

  Type type() const override { return Type::Query; }
  void Load(const QByteArray &data) override { Q_UNUSED(data); }
  QByteArray Save() const override { return QByteArray(); }
  bool is_dynamic() const override { return true; }
  int GetDynamicFuture() override { return future_; }

  PlaylistItemPtrList Generate() override {
    if (block_generate_) release_.acquire();
    return MakeItems(future_ + 1);
  }

  PlaylistItemPtrList GenerateMore(const int count) override {
    requested_.fetchAndAddRelaxed(count);
    const int active = active_.fetchAndAddRelaxed(1) + 1;
    int max_active = max_active_.loadRelaxed();
    while (active > max_active && !max_active_.testAndSetRelaxed(max_active, active, max_active)) {}
    release_.acquire();
    const PlaylistItemPtrList items = generate_more_empty_ ? PlaylistItemPtrList() : MakeItems(count);
    active_.fetchAndSubRelaxed(1);
    return items;
  }

  void Release(const int count) { release_.release(count); }
  int requested() const { return requested_.loadRelaxed(); }
  int max_active() const { return max_active_.loadRelaxed(); }

 private:
  PlaylistItemPtrList MakeItems(const int count) {
    PlaylistItemPtrList items;
    for (int i = 0; i < count; ++i) {
      Song song(Song::Source::Stream);
      const QString title = u"Song %1"_s.arg(next_.fetchAndAddRelaxed(1));
      song.Init(title, u"Artist"_s, u"Album"_s, 123);
      song.set_url(QUrl(u"http://example.com/"_s + title));
      items << PlaylistItem::NewFromSong(song);
    }
    return items;
  }

  const int future_;
  const bool block_generate_;
  const bool generate_more_empty_;
  QSemaphore release_;
  QAtomicInt requested_;
  QAtomicInt active_;
  QAtomicInt max_active_;
  QAtomicInt next_;
};

// Declared at file scope (rather than inside the anonymous namespace below, where the TEST_F-generated fixture subclasses live) so that Playlist's "friend class PlaylistTest;" resolves to this exact class.
// C++ friendship isn't inherited, so tests that need access to Playlist's private members go through the CallXxx() helpers below rather than calling them directly from a TEST_F body.
class PlaylistTest : public ::testing::Test {
 protected:
  // tagreader_client_/tagreader_client_thread_ are declared (and so, per C++ member initialization order, constructed) before playlist_ below: Playlist::tagreader_client_ is a const member set once at construction time, from the SharedPtr this fixture passes in here - so the real TagReaderClient must already exist by then.
  // ReloadItem()'s background task takes that same SharedPtr through Playlist rather than reaching for a process-wide singleton, so constructing and tearing this down per test (like tagreader_test.cpp does) is safe - there is no shared global state left for one test's teardown to leave dangling for another.
  PlaylistTest()
      : tagreader_client_(new TagReaderClient()),
        tagreader_client_thread_(new QThread()),
        playlist_(nullptr, nullptr, nullptr, nullptr, tagreader_client_, 1),
        sequence_(nullptr, new DummySettingsProvider) {
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  ~PlaylistTest() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  void SetUp() override {
    playlist_.set_sequence(&sequence_);
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

  PlaylistItemPtr MakeMockItemWithGroupingP(const QString &title, const QString &grouping) const {
    Song metadata;
    metadata.Init(title, u"Artist"_s, u"Album"_s, 123);
    metadata.set_grouping(grouping);

    MockPlaylistItem *ret = new MockPlaylistItem;
    EXPECT_CALL(*ret, OriginalMetadata()).WillRepeatedly(Return(metadata));

    return PlaylistItemPtr(ret);
  }

  // Forwards to the private Playlist::ReloadItemComplete(), to let tests exercise the save-generation staleness check directly instead of via a real asynchronous write-then-reread round trip.
  void CallReloadItemComplete(const QPersistentModelIndex &idx, const PlaylistItemPtr &item, const Song &new_metadata, const bool saved, const quint64 save_generation, const Song &fallback_metadata = Song()) {
    playlist_.ReloadItemComplete(idx, item, new_metadata, saved, save_generation, fallback_metadata);
  }

  // Forwards to the private Playlist::SaveItemComplete(), to let tests exercise the write-failure path directly instead of via a real asynchronous tag write. On failure this now triggers a real (asynchronous) ReloadItem(), so callers must pump the event loop (see WaitForEditingFinished()) for the result to apply.
  void CallSaveItemComplete(TagReaderReplyPtr reply, const QPersistentModelIndex &idx, const PlaylistItemPtr &item, const quint64 save_generation, const Song &pre_edit_metadata) {
    playlist_.SaveItemComplete(reply, idx, item, save_generation, pre_edit_metadata);
  }

  // Expose the private sort-state fields, to let tests check that undo/redo of a sort restores them (not just the item order).
  bool IsSorted() const { return playlist_.is_sorted_; }
  Playlist::Column SortColumn() const { return playlist_.sort_column_; }
  Qt::SortOrder SortOrder() const { return playlist_.sort_order_; }

  // Replaces the shuffled play order with a known one, so tests of shuffle behavior are deterministic.
  void SetVirtualOrder(const QList<int> &virtual_items, const int current_virtual_index) {
    playlist_.virtual_items_ = virtual_items;
    playlist_.current_virtual_index_ = current_virtual_index;
  }

  // Returns the shuffled play order, to let tests check that it still refers to the same items after the rows changed.
  QList<int> VirtualOrder() const { return playlist_.virtual_items_; }

  // Plays the next rows like the player does, until there is no next row or count rows were played, and returns the played rows.
  // Each next row is asked for twice, like Player::TrackAboutToEnd() does, so a next_row() that changes the result between calls fails the test.
  QList<int> PlayNextRows(const int count) {
    QList<int> rows;
    for (int i = 0; i < count; ++i) {
      const int row = playlist_.next_row();
      if (row == -1) break;
      EXPECT_EQ(row, playlist_.next_row());
      rows << row;
      playlist_.set_current_row(row);
    }
    return rows;
  }

  static QList<int> Sorted(QList<int> list) {
    std::sort(list.begin(), list.end());
    return list;
  }

  // Exposes the private count of dynamic playlist items which are being generated.
  static int DynamicItemsPending(const Playlist &playlist) { return playlist.dynamic_items_pending_; }

  // Processes events, including deferred deletes, until the condition is true, bounded by timeout_ms.
  static bool WaitFor(const std::function<bool()> &condition, const int timeout_ms = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > timeout_ms) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    return true;
  }

  // Forwards to the private Playlist::RemoveItemsNotInQueue(), which is otherwise only reachable through repopulating a dynamic playlist.
  void CallRemoveItemsNotInQueue() {
    playlist_.RemoveItemsNotInQueue();
  }

  // Forwards to the private Playlist::removeRows(QList<int>&), which removes scattered rows as one undo step.
  bool CallRemoveRows(QList<int> &rows) {
    return playlist_.removeRows(rows);
  }

  // Blocks until the given playlist emits RestoreFinished, bounded by timeout_ms like WaitForEditingFinished().
  static void WaitForRestoreFinished(Playlist *playlist, const int timeout_ms = 5000) {
    QEventLoop loop;
    QObject::connect(playlist, &Playlist::RestoreFinished, &loop, &QEventLoop::quit);
    bool timed_out = false;
    QTimer::singleShot(timeout_ms, &loop, [&loop, &timed_out]() {
      timed_out = true;
      loop.quit();
    });
    loop.exec();
    if (timed_out) {
      FAIL() << "Timed out after " << timeout_ms << " ms waiting for Playlist::RestoreFinished";
    }
  }

  static PlaylistItemPtr MakeStreamItem(const QString &title) {
    Song song(Song::Source::Stream);
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl(u"http://example.com/"_s + title));
    return PlaylistItem::NewFromSong(song);
  }

  static QString TitleAt(const Playlist &playlist, const int row) {
    return playlist.data(playlist.index(row, static_cast<int>(Playlist::Column::Title))).toString();
  }

  // Blocks until Playlist::EditingFinished fires, i.e. until an in-flight ReloadItem()'s background reload has completed and ReloadItemComplete() has run.
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

  // Declaration order matters here: these must precede playlist_ so they are constructed first - see the comment above the constructor.
  SharedPtr<TagReaderClient> tagreader_client_;
  QThread *tagreader_client_thread_;

  Playlist playlist_;
  PlaylistSequence sequence_;

};

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
  // The previous track is the one before the removed track, and the next track is the one after it.
  EXPECT_EQ(0, playlist_.previous_row());
  EXPECT_EQ(u"One"_s, TitleAt(playlist_, playlist_.previous_row()));
  EXPECT_EQ(1, playlist_.next_row());
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, playlist_.next_row()));

}

TEST_F(PlaylistTest, RemoveCurrentAtFirstRow) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));

  playlist_.set_current_row(0);
  playlist_.removeRow(0, QModelIndex());
  EXPECT_EQ(-1, playlist_.current_row());
  EXPECT_EQ(0, playlist_.next_row());
  EXPECT_EQ(u"Two"_s, TitleAt(playlist_, playlist_.next_row()));

}

TEST_F(PlaylistTest, RemoveRowsAfterRemovedCurrentKeepsPosition) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));

  playlist_.set_current_row(1);
  playlist_.removeRow(1, QModelIndex());
  ASSERT_EQ(u"Three"_s, TitleAt(playlist_, playlist_.next_row()));

  // Removing a row after the next track does not move the position.
  playlist_.removeRow(3, QModelIndex());
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, playlist_.next_row()));

  // Removing a row before the position moves it with the rows.
  playlist_.removeRow(0, QModelIndex());
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, playlist_.next_row()));

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

TEST_F(PlaylistTest, InsertAtRowPastEnd) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));

  // A row which was valid when loading started, but rows were removed in the meantime.
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Four"_s), 10);
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Four"_s, playlist_.data(playlist_.index(3, static_cast<int>(Playlist::Column::Title))));

  playlist_.undo_stack()->undo();
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Three"_s, playlist_.data(playlist_.index(2, static_cast<int>(Playlist::Column::Title))));

  playlist_.undo_stack()->redo();
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Four"_s, playlist_.data(playlist_.index(3, static_cast<int>(Playlist::Column::Title))));

}

TEST_F(PlaylistTest, DynamicPlaylistAdvancingWhileGeneratingDoesNotGenerateTooMany) {

  SharedPtr<TaskManager> task_manager = std::make_shared<TaskManager>();
  Playlist playlist(task_manager, nullptr, nullptr, nullptr, tagreader_client_, 2);
  playlist.set_sequence(&sequence_);

  constexpr int kFuture = 3;
  SharedPtr<BlockingDynamicGenerator> generator = std::make_shared<BlockingDynamicGenerator>(kFuture);
  playlist.InsertSmartPlaylist(generator);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == kFuture + 1; }));
  ASSERT_TRUE(playlist.is_dynamic());

  // Advancing generates one more item, which blocks in the generator.
  playlist.set_current_row(0);
  playlist.set_current_row(1);
  EXPECT_EQ(1, DynamicItemsPending(playlist));

  // Advancing again before that item is inserted should only generate one more item, not the pending one again.
  // It is generated after the first one is inserted, so the items are inserted in the order they were requested.
  playlist.set_current_row(2);
  EXPECT_EQ(2, DynamicItemsPending(playlist));
  // GenerateMore() runs in a concurrent thread, so wait until it started.
  EXPECT_TRUE(WaitFor([&generator]() { return generator->requested() > 0; }));
  EXPECT_EQ(1, generator->requested());

  // The items are no longer pending when they are inserted, otherwise advancing while inserting counts them twice.
  QList<int> pending_when_inserted;
  QObject::connect(&playlist, &Playlist::rowsInserted, &playlist, [&playlist, &pending_when_inserted]() { pending_when_inserted << DynamicItemsPending(playlist); });

  generator->Release(2);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == 2 + 1 + kFuture; }));
  EXPECT_EQ(0, DynamicItemsPending(playlist));
  EXPECT_EQ(QList<int>() << 1 << 0, pending_when_inserted);

  EXPECT_EQ(2, generator->requested());
  EXPECT_EQ(1, generator->max_active());
  // The history, the current item and the future, the generated items in the order they were generated.
  ASSERT_EQ(2 + 1 + kFuture, playlist.rowCount());
  EXPECT_EQ(u"Song 4"_s, TitleAt(playlist, 4));
  EXPECT_EQ(u"Song 5"_s, TitleAt(playlist, 5));

}

TEST_F(PlaylistTest, DynamicPlaylistAdvancingWhileInitialGenerationDoesNotGenerateMore) {

  SharedPtr<TaskManager> task_manager = std::make_shared<TaskManager>();
  Playlist playlist(task_manager, nullptr, nullptr, nullptr, tagreader_client_, 2);
  playlist.set_sequence(&sequence_);

  // A playlist which is already playing.
  playlist.InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"One"_s) << MakeStreamItem(u"Two"_s));
  playlist.set_current_row(0);

  // Inserting a dynamic playlist generates its future, which blocks in the generator.
  constexpr int kFuture = 3;
  SharedPtr<BlockingDynamicGenerator> generator = std::make_shared<BlockingDynamicGenerator>(kFuture, true);
  playlist.InsertSmartPlaylist(generator);
  ASSERT_TRUE(playlist.is_dynamic());
  EXPECT_EQ(kFuture, DynamicItemsPending(playlist));

  // Advancing before the future is generated should not generate it again.
  playlist.set_current_row(1);
  EXPECT_EQ(0, generator->requested());

  // Release one more than needed, so generating more by mistake fails the test instead of blocking forever.
  generator->Release(2);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == 2 + kFuture + 1; }));
  EXPECT_EQ(0, DynamicItemsPending(playlist));
  EXPECT_EQ(0, generator->requested());

}

TEST_F(PlaylistTest, DynamicPlaylistReplacedWhileGeneratingDoesNotCountOldItems) {

  SharedPtr<TaskManager> task_manager = std::make_shared<TaskManager>();
  Playlist playlist(task_manager, nullptr, nullptr, nullptr, tagreader_client_, 2);
  playlist.set_sequence(&sequence_);

  constexpr int kFuture = 3;
  SharedPtr<BlockingDynamicGenerator> old_generator = std::make_shared<BlockingDynamicGenerator>(kFuture);
  playlist.InsertSmartPlaylist(old_generator);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == kFuture + 1; }));

  // Advancing generates one more item for the old dynamic playlist, which blocks in the generator.
  playlist.set_current_row(0);
  playlist.set_current_row(1);
  EXPECT_EQ(1, DynamicItemsPending(playlist));

  // Replacing the dynamic playlist does not count the item still being generated for the old one.
  SharedPtr<BlockingDynamicGenerator> new_generator = std::make_shared<BlockingDynamicGenerator>(kFuture);
  playlist.InsertSmartPlaylist(new_generator);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == 2 * (kFuture + 1); }));
  EXPECT_EQ(0, DynamicItemsPending(playlist));

  playlist.ExpandDynamicPlaylist();
  EXPECT_EQ(5, DynamicItemsPending(playlist));

  // Finishing the old request does not change the count for the new dynamic playlist.
  old_generator->Release(1);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == 2 * (kFuture + 1) + 1; }));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_EQ(5, DynamicItemsPending(playlist));

  new_generator->Release(1);
  ASSERT_TRUE(WaitFor([&playlist]() { return DynamicItemsPending(playlist) == 0; }));
  EXPECT_EQ(2 * (kFuture + 1) + 1 + 5, playlist.rowCount());

}

TEST_F(PlaylistTest, DynamicPlaylistReplacedWhileGeneratingIsNotTurnedOffByOldEmptyResult) {

  SharedPtr<TaskManager> task_manager = std::make_shared<TaskManager>();
  Playlist playlist(task_manager, nullptr, nullptr, nullptr, tagreader_client_, 2);
  playlist.set_sequence(&sequence_);

  // The old dynamic playlist has no more items.
  constexpr int kFuture = 3;
  SharedPtr<BlockingDynamicGenerator> old_generator = std::make_shared<BlockingDynamicGenerator>(kFuture, false, true);
  playlist.InsertSmartPlaylist(old_generator);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == kFuture + 1; }));

  playlist.set_current_row(0);
  playlist.set_current_row(1);
  // GenerateMore() runs in a concurrent thread, so wait until it started.
  EXPECT_TRUE(WaitFor([&old_generator]() { return old_generator->requested() == 1; }));

  SharedPtr<BlockingDynamicGenerator> new_generator = std::make_shared<BlockingDynamicGenerator>(kFuture);
  playlist.InsertSmartPlaylist(new_generator);
  ASSERT_TRUE(WaitFor([&playlist]() { return playlist.rowCount() == 2 * (kFuture + 1); }));

  // The old request finishes without items, which turns off the old dynamic playlist, but not the new one.
  old_generator->Release(1);
  ASSERT_TRUE(WaitFor([&task_manager]() { return task_manager->GetTasks().isEmpty(); }));
  EXPECT_TRUE(playlist.is_dynamic());
  EXPECT_EQ(2 * (kFuture + 1), playlist.rowCount());

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
  CallReloadItemComplete(idx, item, metadata_edit_two, true, generation_edit_two);
  EXPECT_EQ(u"EditTwoArtist"_s, item->OriginalMetadata().artist());

  // The first edit's reload, started earlier but slower, completes afterwards. Its captured generation no longer matches the item's current generation, so this stale completion must be discarded rather than clobbering the newer edit.
  CallReloadItemComplete(idx, item, metadata_edit_one, true, generation_edit_one);
  EXPECT_EQ(u"EditTwoArtist"_s, item->OriginalMetadata().artist());

}

TEST_F(PlaylistTest, StalePlainReloadCompletionDoesNotClobberInFlightEdit) {

  Song song;
  song.Init(u"Title"_s, u"OriginalArtist"_s, u"Album"_s, 123);
  song.set_url(QUrl::fromLocalFile(u"/tmp/does-not-need-to-exist.mp3"_s));

  PlaylistItemPtr item = std::make_shared<SongPlaylistItem>(song, false);
  playlist_.InsertItems(PlaylistItemPtrList() << item, -1);
  const QPersistentModelIndex idx(playlist_.index(0, static_cast<int>(Playlist::Column::Artist)));

  ASSERT_EQ(u"OriginalArtist"_s, item->OriginalMetadata().artist());

  // Simulate a plain reload (e.g. ReloadItems()/RescanSongs()) starting - it snapshots the item's generation at kick-off, exactly as Playlist::ReloadItem() does for a non-save-triggered reload.
  const quint64 generation_at_reload_start = item->save_generation();

  // Before that reload completes, the user edits the same cell, which bumps the item's save generation, exactly as setData() does.
  item->BumpSaveGeneration();
  Song edited_metadata = item->OriginalMetadata();
  edited_metadata.set_artist(u"EditedArtist"_s);
  playlist_.UpdateItemMetadata(0, item, edited_metadata, false);
  ASSERT_EQ(u"EditedArtist"_s, item->OriginalMetadata().artist());

  // The plain reload, started before the edit, now completes with saved=false and its stale, pre-edit generation. It must not clobber the newer edit just because it wasn't itself a save-triggered reload.
  Song stale_reloaded_metadata = song;
  stale_reloaded_metadata.set_artist(u"ReloadedOriginalArtist"_s);
  CallReloadItemComplete(idx, item, stale_reloaded_metadata, false, generation_at_reload_start);
  EXPECT_EQ(u"EditedArtist"_s, item->OriginalMetadata().artist());

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

  // The write fails, and since the file doesn't exist, the reload SaveItemComplete() triggers to resync with disk will fail too.
  TagReaderReplyPtr reply(new TagReaderReply(song.url().toLocalFile()));
  reply->set_result(TagReaderResult(TagReaderResult::ErrorCode::FileSaveError));

  CallSaveItemComplete(reply, idx, item, save_generation, pre_edit_metadata);
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
    TagReaderReplyPtr write_reply = tagreader_client_->WriteFileAsync(resource.fileName(), baseline);
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

  CallSaveItemComplete(reply, idx, item, generation_edit_two, pre_edit_metadata_two);
  WaitForEditingFinished();

  // The item must reflect what is genuinely on disk ("RealDiskArtist"), not edit 1's stale, never-confirmed optimistic value ("EditOneArtist") that pre_edit_metadata_two happened to carry.
  EXPECT_EQ(u"RealDiskArtist"_s, item->OriginalMetadata().artist());

}

// Regression test: clearing the sort indicator (column -1, e.g. from resetting columns to their defaults) stops the playlist from tracking itself as sorted, but must not reorder it - there's no "restore the pre-sort order" behavior.
TEST_F(PlaylistTest, ClearingSortIndicatorStopsTrackingAsSortedWithoutReordering) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"B"_s) << MakeMockItemP(u"A"_s), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  ASSERT_TRUE(IsSorted());
  ASSERT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  ASSERT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  playlist_.sort(-1, Qt::AscendingOrder);
  EXPECT_FALSE(IsSorted());
  // Order is left exactly as it was (A, B), not reverted to the pre-sort B, A order.
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

}

// Regression test for #1690: "Automatically sort playlist when inserting songs" must not act as if the playlist were sorted by the (arbitrary) default column until the user has actually chosen a sort column by clicking a header.
TEST_F(PlaylistTest, AutoSortDoesNothingUntilAColumnHasBeenSorted) {

  playlist_.set_auto_sort(true);

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"B"_s) << MakeMockItemP(u"A"_s), -1);
  // No column has been sorted yet, so auto-sort must leave the insertion order alone.
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  ASSERT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  ASSERT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  // Now that the playlist has an active sort column, auto-sort should kick in and place new insertions in order.
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"AB"_s), -1);
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"AB"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(2, static_cast<int>(Playlist::Column::Title))));

}

// Regression test: with auto-sort, undoing an insert must remove the inserted items rather than whatever ended up at the insert rows after sorting, and redo must insert and sort them again.
// The auto-sort used to push its sort command from inside the insert command, which put the commands on the undo stack in the wrong order.
TEST_F(PlaylistTest, UndoRedoAutoSortedInsert) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"D"_s) << MakeMockItemP(u"B"_s), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  playlist_.set_auto_sort(true);

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"C"_s) << MakeMockItemP(u"A"_s), -1);
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"C"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 3));

  // One undo step reverts both the insert and its auto-sort.
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  playlist_.undo_stack()->undo();
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 1));
  EXPECT_TRUE(IsSorted());

  ASSERT_TRUE(playlist_.undo_stack()->canRedo());
  playlist_.undo_stack()->redo();
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"C"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 3));

  // Undo the insert again, then the original sort.
  playlist_.undo_stack()->undo();
  playlist_.undo_stack()->undo();
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_FALSE(IsSorted());

}

// Regression test: when an auto-sorted insert's item is replaced after loading, redo must bring back the loaded item rather than the one stored by the sort command.
TEST_F(PlaylistTest, RedoAutoSortedInsertKeepsLoadedItem) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"D"_s) << MakeMockItemP(u"B"_s), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  playlist_.set_auto_sort(true);

  Song song;
  song.Init(u"C"_s, u"Artist"_s, u"Album"_s, 123);
  song.set_url(QUrl::fromLocalFile(u"/tmp/does-not-need-to-exist.mp3"_s));
  playlist_.InsertItems(PlaylistItemPtrList() << std::make_shared<SongPlaylistItem>(song), -1);
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  ASSERT_EQ(u"C"_s, TitleAt(playlist_, 1));

  // Replace the item, like SongLoaderInserter does when the songs have finished loading.
  Song loaded_song = song;
  loaded_song.set_title(u"C loaded"_s);
  playlist_.UpdateItems(SongList() << loaded_song);
  ASSERT_EQ(u"C loaded"_s, TitleAt(playlist_, 1));
  const PlaylistItemPtr loaded_item = playlist_.item_at(1);

  playlist_.undo_stack()->undo();
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));

  playlist_.undo_stack()->redo();
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"C loaded"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(loaded_item, playlist_.item_at(1));
  EXPECT_EQ(1, playlist_.IndexByUuId(loaded_item->uuid()));

}

// Regression test: updating items must give the songs with the same URL, like the tracks of a CUE sheet, to the items with that URL in order, leave items without a song alone, and replace the items in the sort commands.
TEST_F(PlaylistTest, UpdateItemsMatchesSongsByUrlInOrder) {

  const auto make_song = [](const QString &title, const QString &filename) {
    Song song;
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl::fromLocalFile(u"/tmp/"_s + filename));
    return song;
  };

  playlist_.InsertItems(PlaylistItemPtrList()
                        << std::make_shared<SongPlaylistItem>(make_song(u"A"_s, u"image.flac"_s))
                        << std::make_shared<SongPlaylistItem>(make_song(u"B"_s, u"other.flac"_s))
                        << std::make_shared<SongPlaylistItem>(make_song(u"C"_s, u"image.flac"_s)), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::DescendingOrder);
  ASSERT_EQ(u"C"_s, TitleAt(playlist_, 0));
  ASSERT_EQ(u"B"_s, TitleAt(playlist_, 1));
  ASSERT_EQ(u"A"_s, TitleAt(playlist_, 2));

  // The two loaded tracks of image.flac are used for the items with that URL in playlist order, other.flac has no song.
  playlist_.UpdateItems(SongList() << make_song(u"Track 1"_s, u"image.flac"_s) << make_song(u"Track 2"_s, u"image.flac"_s));
  EXPECT_EQ(u"Track 1"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"Track 2"_s, TitleAt(playlist_, 2));

  // Undoing and redoing the sort keeps the loaded items.
  playlist_.undo_stack()->undo();
  EXPECT_EQ(u"Track 2"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"Track 1"_s, TitleAt(playlist_, 2));
  playlist_.undo_stack()->redo();
  EXPECT_EQ(u"Track 1"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"Track 2"_s, TitleAt(playlist_, 2));

  // Undoing and redoing the insert keeps each loaded item too, also for the items sharing a URL.
  playlist_.undo_stack()->undo();
  playlist_.undo_stack()->undo();
  ASSERT_EQ(0, playlist_.rowCount(QModelIndex()));
  playlist_.undo_stack()->redo();
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Track 2"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"Track 1"_s, TitleAt(playlist_, 2));

}

// Regression test: an auto-sorted insert too large for the undo stack must still be sorted, and must leave the undo stack empty.
TEST_F(PlaylistTest, LargeAutoSortedInsertIsSortedAndClearsUndoStack) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"B"_s) << MakeMockItemP(u"A"_s), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  playlist_.set_auto_sort(true);

  // Insert in descending order, so the result is only in order if it was sorted.
  PlaylistItemPtrList items;
  const int count = Playlist::kUndoItemLimit + 1;
  for (int i = count - 1; i >= 0; --i) {
    items << MakeMockItemP(u"T%1"_s.arg(i, 4, 10, QLatin1Char('0')));
  }
  playlist_.InsertItems(items, -1);

  ASSERT_EQ(count + 2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  for (int i = 0; i < count; ++i) {
    EXPECT_EQ(u"T%1"_s.arg(i, 4, 10, QLatin1Char('0')), TitleAt(playlist_, i + 2));
  }
  EXPECT_EQ(0, playlist_.undo_stack()->count());

}

// Regression test: with auto-sort, undoing a removal must put the item back at its row without pushing a sort command onto the undo stack, and redo must remove the same item again.
TEST_F(PlaylistTest, UndoRedoRemoveWithAutoSort) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"C"_s) << MakeMockItemP(u"A"_s) << MakeMockItemP(u"B"_s), -1);
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  playlist_.set_auto_sort(true);
  const int undo_count = playlist_.undo_stack()->count();

  ASSERT_TRUE(playlist_.removeRows(1, 1, QModelIndex()));
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"C"_s, TitleAt(playlist_, 1));

  playlist_.undo_stack()->undo();
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"C"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(undo_count + 1, playlist_.undo_stack()->count());

  playlist_.undo_stack()->redo();
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"C"_s, TitleAt(playlist_, 1));

}

// Regression test: undoing a column sort must also revert the playlist's own idea of what's sorted (is_sorted_/sort_column_/sort_order_), and announce it via SortStateChanged, so the header's sort indicator can be kept in sync rather than being left showing a sort that's since been undone.
TEST_F(PlaylistTest, UndoingASortAlsoRevertsSortState) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"B"_s) << MakeMockItemP(u"A"_s), -1);

  bool signal_emitted = false;
  bool signal_is_sorted = true;
  Playlist::Column signal_column = Playlist::Column::Artist;
  Qt::SortOrder signal_sort_order = Qt::DescendingOrder;
  QObject::connect(&playlist_, &Playlist::SortStateChanged, [&](const bool is_sorted, const Playlist::Column column, const Qt::SortOrder sort_order) {
    signal_emitted = true;
    signal_is_sorted = is_sorted;
    signal_column = column;
    signal_sort_order = sort_order;
  });

  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  ASSERT_TRUE(IsSorted());
  ASSERT_EQ(Playlist::Column::Title, SortColumn());
  ASSERT_EQ(Qt::AscendingOrder, SortOrder());
  ASSERT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  ASSERT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  // Undo: back to unsorted, with the pre-sort B, A order restored.
  playlist_.undo_stack()->undo();

  EXPECT_TRUE(signal_emitted);
  EXPECT_FALSE(signal_is_sorted);
  EXPECT_FALSE(IsSorted());
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  // Redo: sorted state (and order) comes back too, not just the item order.
  signal_emitted = false;
  playlist_.undo_stack()->redo();

  EXPECT_TRUE(signal_emitted);
  EXPECT_TRUE(signal_is_sorted);
  EXPECT_EQ(Playlist::Column::Title, signal_column);
  EXPECT_EQ(Qt::AscendingOrder, signal_sort_order);
  EXPECT_TRUE(IsSorted());
  EXPECT_EQ(Playlist::Column::Title, SortColumn());
  EXPECT_EQ(Qt::AscendingOrder, SortOrder());
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

}

// Regression test: moving non-contiguous rows to another playlist must remove exactly those rows from the source playlist.
// The rows used to be removed in ascending order, so every removal shifted the rows after it and the wrong items were removed.
TEST_F(PlaylistTest, MoveNonContiguousRowsToAnotherPlaylist) {

  Playlist source_playlist(nullptr, nullptr, nullptr, nullptr, tagreader_client_, 2);
  source_playlist.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));
  ASSERT_EQ(5, source_playlist.rowCount(QModelIndex()));

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Destination"_s));
  ASSERT_EQ(1, playlist_.rowCount(QModelIndex()));

  // Move "One", "Three" and "Five", including the last row, to the end of the destination playlist.
  const QModelIndexList source_indexes = QModelIndexList() << source_playlist.index(0, 0) << source_playlist.index(2, 0) << source_playlist.index(4, 0);
  ScopedPtr<QMimeData> mimedata(source_playlist.mimeData(source_indexes));
  ASSERT_TRUE(mimedata);
  ASSERT_TRUE(playlist_.dropMimeData(mimedata.get(), Qt::MoveAction, -1, 0, QModelIndex()));

  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Destination"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"One"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Three"_s, playlist_.data(playlist_.index(2, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Five"_s, playlist_.data(playlist_.index(3, static_cast<int>(Playlist::Column::Title))));

  ASSERT_EQ(2, source_playlist.rowCount(QModelIndex()));
  EXPECT_EQ(u"Two"_s, source_playlist.data(source_playlist.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Four"_s, source_playlist.data(source_playlist.index(1, static_cast<int>(Playlist::Column::Title))));

  // Undoing the removal in the source playlist puts the items back at their original rows.
  source_playlist.undo_stack()->undo();
  ASSERT_EQ(5, source_playlist.rowCount(QModelIndex()));
  EXPECT_EQ(u"One"_s, source_playlist.data(source_playlist.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Two"_s, source_playlist.data(source_playlist.index(1, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Three"_s, source_playlist.data(source_playlist.index(2, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Four"_s, source_playlist.data(source_playlist.index(3, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"Five"_s, source_playlist.data(source_playlist.index(4, static_cast<int>(Playlist::Column::Title))));

}

// Regression test: undoing a move within a playlist must keep persistent indexes on the same items, including unmoved items that end up at one of the restored rows.
TEST_F(PlaylistTest, UndoMoveKeepsPersistentIndexesOnSameItems) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));
  ASSERT_EQ(5, playlist_.rowCount(QModelIndex()));

  // Drag "One" and "Three" within the playlist to before "Five".
  const QModelIndexList source_indexes = QModelIndexList() << playlist_.index(0, 0) << playlist_.index(2, 0);
  ScopedPtr<QMimeData> mimedata(playlist_.mimeData(source_indexes));
  ASSERT_TRUE(mimedata);
  ASSERT_TRUE(playlist_.dropMimeData(mimedata.get(), Qt::MoveAction, 4, 0, QModelIndex()));

  const QStringList moved_titles = QStringList() << u"Two"_s << u"Four"_s << u"One"_s << u"Three"_s << u"Five"_s;
  ASSERT_EQ(5, playlist_.rowCount(QModelIndex()));
  for (int row = 0; row < moved_titles.count(); ++row) {
    EXPECT_EQ(moved_titles[row], playlist_.data(playlist_.index(row, static_cast<int>(Playlist::Column::Title))));
  }

  QList<QPersistentModelIndex> persistent_indexes;
  for (int row = 0; row < playlist_.rowCount(QModelIndex()); ++row) {
    persistent_indexes << QPersistentModelIndex(playlist_.index(row, static_cast<int>(Playlist::Column::Title)));
  }
  playlist_.set_current_row(0);

  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  playlist_.undo_stack()->undo();

  const QStringList original_titles = QStringList() << u"One"_s << u"Two"_s << u"Three"_s << u"Four"_s << u"Five"_s;
  ASSERT_EQ(5, playlist_.rowCount(QModelIndex()));
  for (int row = 0; row < original_titles.count(); ++row) {
    EXPECT_EQ(original_titles[row], playlist_.data(playlist_.index(row, static_cast<int>(Playlist::Column::Title))));
  }

  // Each persistent index still points at the item it pointed at before the undo.
  for (int i = 0; i < persistent_indexes.count(); ++i) {
    ASSERT_TRUE(persistent_indexes[i].isValid());
    EXPECT_EQ(moved_titles[i], persistent_indexes[i].data());
  }
  EXPECT_EQ(1, playlist_.current_row());
  EXPECT_EQ(u"Two"_s, playlist_.current_item()->EffectiveMetadata().title());

}

// Regression test: copying rows to another playlist must insert new items with new UUIDs, not share the source playlist's items, even when the same rows are copied twice.
TEST_F(PlaylistTest, CopyRowsToAnotherPlaylistInsertsNewItems) {

  Playlist source_playlist(nullptr, nullptr, nullptr, nullptr, tagreader_client_, 2);
  source_playlist.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));
  ASSERT_EQ(3, source_playlist.rowCount(QModelIndex()));

  const QModelIndexList source_indexes = QModelIndexList() << source_playlist.index(0, 0) << source_playlist.index(2, 0);
  for (int i = 0; i < 2; ++i) {
    ScopedPtr<QMimeData> mimedata(source_playlist.mimeData(source_indexes));
    ASSERT_TRUE(mimedata);
    ASSERT_TRUE(playlist_.dropMimeData(mimedata.get(), Qt::CopyAction, -1, 0, QModelIndex()));
  }

  // The source playlist is left alone.
  ASSERT_EQ(3, source_playlist.rowCount(QModelIndex()));

  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"One"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"One"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, 3));

  // Every row is its own item with its own UUID, not shared with the source playlist or with another row.
  QSet<QUuid> uuids;
  for (int row = 0; row < source_playlist.rowCount(QModelIndex()); ++row) {
    uuids << source_playlist.item_at(row)->uuid();
  }
  for (int row = 0; row < playlist_.rowCount(QModelIndex()); ++row) {
    const PlaylistItemPtr item = playlist_.item_at(row);
    for (int source_row = 0; source_row < source_playlist.rowCount(QModelIndex()); ++source_row) {
      EXPECT_NE(source_playlist.item_at(source_row), item);
    }
    EXPECT_FALSE(uuids.contains(item->uuid()));
    uuids << item->uuid();
    EXPECT_EQ(row, playlist_.IndexByUuId(item->uuid()));
  }

  // Removing one copy must not affect looking up the other copy of the same row by UUID.
  const QUuid remaining_uuid = playlist_.item_at(2)->uuid();
  playlist_.removeRow(0);
  EXPECT_EQ(1, playlist_.IndexByUuId(remaining_uuid));

}

// Regression test: with auto-sort, "play now" must request the first inserted item, not whatever item was sorted into the insert position.
TEST_F(PlaylistTest, PlayNowWithAutoSortRequestsFirstInsertedItem) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"D"_s) << MakeMockItemP(u"B"_s));
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  playlist_.set_auto_sort(true);
  ASSERT_EQ(u"B"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  ASSERT_EQ(u"D"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));

  int play_requested_count = 0;
  QModelIndex play_requested_index;
  QObject::connect(&playlist_, &Playlist::PlayRequested, &playlist_, [&play_requested_count, &play_requested_index](const QModelIndex idx, const Playlist::AutoScroll autoscroll) {
    Q_UNUSED(autoscroll)
    ++play_requested_count;
    play_requested_index = idx;
  });

  // "E" is appended at row 2, but auto-sort moves it to row 3 and puts "D" at row 2.
  const PlaylistItemPtr item_e = MakeMockItemP(u"E"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_e << MakeMockItemP(u"A"_s), -1, true);

  ASSERT_EQ(4, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"A"_s, playlist_.data(playlist_.index(0, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"B"_s, playlist_.data(playlist_.index(1, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"D"_s, playlist_.data(playlist_.index(2, static_cast<int>(Playlist::Column::Title))));
  EXPECT_EQ(u"E"_s, playlist_.data(playlist_.index(3, static_cast<int>(Playlist::Column::Title))));

  ASSERT_EQ(1, play_requested_count);
  ASSERT_TRUE(play_requested_index.isValid());
  EXPECT_EQ(3, play_requested_index.row());
  EXPECT_EQ(item_e, playlist_.item_at(play_requested_index.row()));

}

// Regression test: with album repeat, removing the current track must still advance to the next track on the same album, using the metadata of the removed track.
TEST_F(PlaylistTest, RemoveCurrentWithRepeatAlbumAdvancesOnSameAlbum) {

  const PlaylistItemPtr item_x1 = MakeMockItemP(u"X1"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y1 = MakeMockItemP(u"Y1"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x2 = MakeMockItemP(u"X2"_s, u"Artist"_s, u"Album X"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_x1 << item_y1 << item_x2);

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Album);

  playlist_.set_current_row(0);
  playlist_.removeRow(0);
  ASSERT_EQ(-1, playlist_.current_row());

  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(item_x2, playlist_.item_at(next_row));

}

// Regression test: with album repeat, removing the current track must still go back to the previous track on the same album.
TEST_F(PlaylistTest, RemoveCurrentWithRepeatAlbumGoesBackOnSameAlbum) {

  const PlaylistItemPtr item_x1 = MakeMockItemP(u"X1"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y1 = MakeMockItemP(u"Y1"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x2 = MakeMockItemP(u"X2"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y2 = MakeMockItemP(u"Y2"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x3 = MakeMockItemP(u"X3"_s, u"Artist"_s, u"Album X"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_x1 << item_y1 << item_x2 << item_y2 << item_x3);

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Album);

  playlist_.set_current_row(4);
  playlist_.removeRow(4);
  ASSERT_EQ(-1, playlist_.current_row());

  const int previous_row = playlist_.previous_row();
  ASSERT_NE(-1, previous_row);
  EXPECT_EQ(item_x2, playlist_.item_at(previous_row));

}

// Regression test: with inside album shuffle, removing the current track must still advance to the next track on the same album in the shuffle order.
TEST_F(PlaylistTest, RemoveCurrentWithInsideAlbumShuffleAdvancesOnSameAlbum) {

  const PlaylistItemPtr item_x1 = MakeMockItemP(u"X1"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y1 = MakeMockItemP(u"Y1"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x2 = MakeMockItemP(u"X2"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y2 = MakeMockItemP(u"Y2"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x3 = MakeMockItemP(u"X3"_s, u"Artist"_s, u"Album X"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_x1 << item_y1 << item_x2 << item_y2 << item_x3);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::InsideAlbum);
  playlist_.set_current_row(0);

  // Shuffle order Y1, X1 (current), Y2, X3, X2: after X1, the next track on album X is X3.
  SetVirtualOrder(QList<int>() << 1 << 0 << 3 << 4 << 2, 1);

  playlist_.removeRow(0);
  ASSERT_EQ(-1, playlist_.current_row());

  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(item_x3, playlist_.item_at(next_row));

}

// Regression test: the metadata of a removed current track must only be used until the current row changes, not after selecting or clearing a new current row.
TEST_F(PlaylistTest, RemovedCurrentMetadataIsResetWhenCurrentRowChanges) {

  const PlaylistItemPtr item_x1 = MakeMockItemP(u"X1"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y1 = MakeMockItemP(u"Y1"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x2 = MakeMockItemP(u"X2"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y2 = MakeMockItemP(u"Y2"_s, u"Artist"_s, u"Album Y"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_x1 << item_y1 << item_x2 << item_y2);

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Album);

  playlist_.set_current_row(0);
  playlist_.removeRow(0);
  ASSERT_EQ(-1, playlist_.current_row());
  ASSERT_NE(-1, playlist_.next_row());
  ASSERT_EQ(item_x2, playlist_.item_at(playlist_.next_row()));

  // Selecting a new current row uses the album of that row.
  playlist_.set_current_row(playlist_.row_of(item_y1));
  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(item_y2, playlist_.item_at(next_row));

  // Clearing the current row must not fall back to the album of the removed track.
  playlist_.set_current_row(-1);
  EXPECT_EQ(-1, playlist_.next_row());

}

// Regression test: with shuffle, removing the current track must keep the position in the shuffle order, so the next track is the one after the removed track.
TEST_F(PlaylistTest, RemoveCurrentWithShuffleKeepsShufflePosition) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 6; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(5);

  // Shuffle order 3, 0, 5 (current), 1, 4, 2.
  SetVirtualOrder(QList<int>() << 3 << 0 << 5 << 1 << 4 << 2, 2);

  playlist_.removeRow(5);
  ASSERT_EQ(-1, playlist_.current_row());

  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(items[1], playlist_.item_at(next_row));

}

// Regression test: with shuffle, inserting several items in the middle of the playlist must keep the played part of the shuffle order, keep the order of the rest, and add each new item once after the current track.
// The new items get random positions, so only these properties are checked, not the exact order.
TEST_F(PlaylistTest, InsertSeveralWithShuffleKeepsShuffleOrder) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 6; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(5);

  // Shuffle order 3, 0, 5 (current), 1, 4, 2.
  SetVirtualOrder(QList<int>() << 3 << 0 << 5 << 1 << 4 << 2, 2);

  // Insert 4 items at row 2, the rows from 2 on move down by 4.
  PlaylistItemPtrList new_items;
  for (int i = 0; i < 4; ++i) {
    new_items << MakeMockItemP(u"New "_s + QString::number(i));
  }
  playlist_.InsertItems(new_items, 2);
  ASSERT_EQ(10, playlist_.rowCount(QModelIndex()));

  const QList<int> virtual_order = VirtualOrder();
  ASSERT_EQ(10, virtual_order.count());

  // The played part and the current track are kept, with the moved rows: 3, 0, 5 became 7, 0, 9.
  EXPECT_EQ((QList<int>() << 7 << 0 << 9), virtual_order.first(3));
  EXPECT_EQ(9, playlist_.current_row());
  EXPECT_EQ(items[5], playlist_.current_item());

  // After the current track, the remaining old rows 1, 4, 2 (now 1, 8, 6) keep their order, and each new row 2 to 5 appears once.
  const QList<int> rest = virtual_order.sliced(3);
  QList<int> old_rows_in_rest;
  QList<int> new_rows_in_rest;
  for (const int row : rest) {
    if (row >= 2 && row <= 5) {
      new_rows_in_rest << row;
    }
    else {
      old_rows_in_rest << row;
    }
  }
  EXPECT_EQ((QList<int>() << 1 << 8 << 6), old_rows_in_rest);
  std::sort(new_rows_in_rest.begin(), new_rows_in_rest.end());
  EXPECT_EQ((QList<int>() << 2 << 3 << 4 << 5), new_rows_in_rest);

}

// Regression test: with shuffle, removing several rows including the current track, with removed items both before and after it in the shuffle order, must keep the position in the shuffle order.
TEST_F(PlaylistTest, RemoveRowsAroundCurrentWithShuffleKeepsShufflePosition) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 6; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(5);

  // Shuffle order 3, 0, 5 (current), 1, 4, 2.
  SetVirtualOrder(QList<int>() << 3 << 0 << 5 << 1 << 4 << 2, 2);

  // Remove rows 3, 4 and 5: row 3 is before the current track in the shuffle order, row 4 is after it.
  playlist_.removeRows(3, 3);
  ASSERT_EQ(3, playlist_.rowCount(QModelIndex()));
  ASSERT_EQ(-1, playlist_.current_row());

  // The remaining shuffle order is 0, 1, 2, and the track after the removed current track is 1.
  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(items[1], playlist_.item_at(next_row));

}

// Regression test: with shuffle, after removing the current track, the previous track is the one before it in the shuffle order, which the retained virtual index points at.
TEST_F(PlaylistTest, RemoveCurrentWithShufflePreviousIsPredecessor) {

  const PlaylistItemPtr item_one = MakeMockItemP(u"One"_s);
  const PlaylistItemPtr item_two = MakeMockItemP(u"Two"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_one << item_two);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(1);

  // Shuffle order One, Two (current).
  SetVirtualOrder(QList<int>() << 0 << 1, 1);

  playlist_.removeRow(1);
  ASSERT_EQ(-1, playlist_.current_row());

  const int previous_row = playlist_.previous_row();
  ASSERT_NE(-1, previous_row);
  EXPECT_EQ(item_one, playlist_.item_at(previous_row));

}

// Regression test: with album repeat, going back from the start of the list after removing the current track wraps around to the last track on the same album, without reading past the end of the list.
TEST_F(PlaylistTest, RemoveCurrentWithRepeatAlbumPreviousWrapsAround) {

  const PlaylistItemPtr item_x1 = MakeMockItemP(u"X1"_s, u"Artist"_s, u"Album X"_s);
  const PlaylistItemPtr item_y1 = MakeMockItemP(u"Y1"_s, u"Artist"_s, u"Album Y"_s);
  const PlaylistItemPtr item_x2 = MakeMockItemP(u"X2"_s, u"Artist"_s, u"Album X"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_x1 << item_y1 << item_x2);

  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Album);

  playlist_.set_current_row(0);
  playlist_.removeRow(0);
  ASSERT_EQ(-1, playlist_.current_row());

  const int previous_row = playlist_.previous_row();
  ASSERT_NE(-1, previous_row);
  EXPECT_EQ(item_x2, playlist_.item_at(previous_row));

}

// Regression test: with shuffle and track repeat, removing the current track must advance to the next track in the shuffle order, not repeat the one before it.
TEST_F(PlaylistTest, RemoveCurrentWithShuffleAndRepeatTrackAdvances) {

  const PlaylistItemPtr item_a = MakeMockItemP(u"A"_s);
  const PlaylistItemPtr item_b = MakeMockItemP(u"B"_s);
  const PlaylistItemPtr item_c = MakeMockItemP(u"C"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_a << item_b << item_c);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Track);
  playlist_.set_current_row(1);

  // Shuffle order A, B (current), C.
  SetVirtualOrder(QList<int>() << 0 << 1 << 2, 1);

  playlist_.removeRow(1);
  ASSERT_EQ(-1, playlist_.current_row());

  const int next_row = playlist_.next_row();
  ASSERT_NE(-1, next_row);
  EXPECT_EQ(item_c, playlist_.item_at(next_row));

  const int previous_row = playlist_.previous_row();
  ASSERT_NE(-1, previous_row);
  EXPECT_EQ(item_a, playlist_.item_at(previous_row));

}

// Regression test: with shuffle and track repeat, removing the current track at the end of the shuffle order must stop, not repeat the track before it.
TEST_F(PlaylistTest, RemoveLastCurrentWithShuffleAndRepeatTrackStops) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"A"_s) << MakeMockItemP(u"B"_s));

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Track);
  playlist_.set_current_row(1);

  // Shuffle order A, B (current).
  SetVirtualOrder(QList<int>() << 0 << 1, 1);

  playlist_.removeRow(1);
  ASSERT_EQ(-1, playlist_.current_row());

  EXPECT_EQ(-1, playlist_.next_row());

}

// Regression test: with shuffle and playlist repeat, the next shuffle order must start after the current track, so every other track is played before any track is repeated.
// The next row used to be asked for twice when preloading, and the second call continued from the current track's random position in the new order, skipping the tracks before it.
TEST_F(PlaylistTest, ShuffleRepeatPlaylistPlaysAllTracksAfterWrapping) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 5; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.sequence()->SetRepeatMode(PlaylistSequence::RepeatMode::Playlist);
  playlist_.set_current_row(0);

  // Row 0 is the last track in the shuffle order, so the next track starts a new shuffle order.
  SetVirtualOrder(QList<int>() << 1 << 2 << 3 << 4 << 0, 4);

  EXPECT_EQ(QList<int>() << 1 << 2 << 3 << 4, Sorted(PlayNextRows(4)));

}

// Regression test: with shuffle, choosing a track that has not been played yet must not skip the tracks before it in the shuffle order, or replay the tracks after it.
TEST_F(PlaylistTest, ShuffleChoosingUnplayedTrackKeepsTheRest) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 5; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(0);
  SetVirtualOrder(QList<int>() << 0 << 1 << 2 << 3 << 4, 0);

  playlist_.set_current_row(3);

  EXPECT_EQ(QList<int>() << 1 << 2 << 4, PlayNextRows(5));

}

// Regression test: with shuffle, going back to the previous track must keep the shuffle order, so the next track is the one that was backed out of.
TEST_F(PlaylistTest, ShufflePreviousThenNextKeepsShuffleOrder) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 4; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(2);
  SetVirtualOrder(QList<int>() << 0 << 1 << 2 << 3, 2);

  // Like Player::PreviousItem().
  const int previous_row = playlist_.take_previous_row();
  ASSERT_EQ(1, previous_row);
  playlist_.set_current_row(previous_row);

  EXPECT_EQ(QList<int>() << 2 << 3, PlayNextRows(4));

}

// Regression test: when the player reshuffles before playing a chosen track, which it does when a track is double-clicked, every other track must still be played once.
TEST_F(PlaylistTest, ShuffleReshuffleThenChoosingTrackPlaysAllTracks) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 6; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  playlist_.set_current_row(0);

  playlist_.ReshuffleIndices();
  playlist_.set_current_row(3);

  EXPECT_EQ(QList<int>() << 1 << 2 << 4 << 5, Sorted(PlayNextRows(6)));

}

// Regression test: with album shuffle, when the player reshuffles before playing a chosen track, the rest of the chosen album and every other album must still be played, keeping the albums together.
TEST_F(PlaylistTest, AlbumShuffleReshuffleThenChoosingTrackPlaysAllAlbums) {

  playlist_.InsertItems(PlaylistItemPtrList()
      << MakeMockItemP(u"A1"_s, u"Artist"_s, u"Album A"_s)
      << MakeMockItemP(u"A2"_s, u"Artist"_s, u"Album A"_s)
      << MakeMockItemP(u"B1"_s, u"Artist"_s, u"Album B"_s)
      << MakeMockItemP(u"B2"_s, u"Artist"_s, u"Album B"_s)
      << MakeMockItemP(u"C1"_s, u"Artist"_s, u"Album C"_s)
      << MakeMockItemP(u"C2"_s, u"Artist"_s, u"Album C"_s));

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::Albums);
  playlist_.set_current_row(0);

  playlist_.ReshuffleIndices();
  playlist_.set_current_row(4);

  // C2 finishes album C, then the rest of album A, then album B.
  EXPECT_EQ(QList<int>() << 5 << 1 << 2 << 3, PlayNextRows(6));

}

// Regression test: with grouping shuffle, when the player reshuffles before playing a chosen track, the rest of the chosen grouping and every other grouping must still be played, keeping the groupings together.
TEST_F(PlaylistTest, GroupingShuffleReshuffleThenChoosingTrackPlaysAllGroupings) {

  playlist_.InsertItems(PlaylistItemPtrList()
      << MakeMockItemWithGroupingP(u"A1"_s, u"Grouping A"_s)
      << MakeMockItemWithGroupingP(u"A2"_s, u"Grouping A"_s)
      << MakeMockItemWithGroupingP(u"B1"_s, u"Grouping B"_s)
      << MakeMockItemWithGroupingP(u"B2"_s, u"Grouping B"_s)
      << MakeMockItemWithGroupingP(u"C1"_s, u"Grouping C"_s)
      << MakeMockItemWithGroupingP(u"C2"_s, u"Grouping C"_s));

  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::Grouping);
  playlist_.set_current_row(0);

  playlist_.ReshuffleIndices();
  playlist_.set_current_row(4);

  // C2 finishes grouping C, then the rest of grouping A, then grouping B.
  EXPECT_EQ(QList<int>() << 5 << 1 << 2 << 3, PlayNextRows(6));

}

// Regression test: turning on shuffle while a track is playing must play every other track once, not only the tracks after the current track's random position.
TEST_F(PlaylistTest, ShuffleTurnedOnWhilePlayingPlaysAllTracks) {

  PlaylistItemPtrList items;
  for (int i = 0; i < 5; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);

  playlist_.set_current_row(2);
  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);

  EXPECT_EQ(QList<int>() << 0 << 1 << 3 << 4, Sorted(PlayNextRows(5)));

}

// Regression test: restoring a playlist must not be undoable, and must clear undo commands added while the restore was in progress, since their rows are no longer valid.
TEST_F(PlaylistTest, RestoreClearsUndoStack) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  // A file database rather than a MemoryDatabase, since the playlist is restored from another thread, and each thread gets its own connection.
  SharedPtr<Database> database = std::make_shared<Database>(nullptr, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  SharedPtr<PlaylistBackend> playlist_backend = std::make_shared<PlaylistBackend>(database, tagreader_client_, nullptr);

  const int playlist_id = playlist_backend->CreatePlaylist(u"Restored"_s, QString());
  ASSERT_NE(-1, playlist_id);
  PlaylistItemSaveDataList items_save_data;
  items_save_data << MakeStreamItem(u"Restored1"_s)->CreateSaveData() << MakeStreamItem(u"Restored2"_s)->CreateSaveData();
  playlist_backend->SavePlaylist(playlist_id, items_save_data, -1, nullptr);

  {
    // The restore starts in the constructor, but the restored items are only inserted from the event loop.
    Playlist playlist(nullptr, nullptr, playlist_backend, nullptr, tagreader_client_, playlist_id);
    ASSERT_EQ(0, playlist.rowCount(QModelIndex()));

    playlist.InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added1"_s));
    playlist.InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added2"_s));
    playlist.undo_stack()->undo();
    ASSERT_EQ(1, playlist.rowCount(QModelIndex()));
    ASSERT_TRUE(playlist.undo_stack()->canUndo());
    ASSERT_TRUE(playlist.undo_stack()->canRedo());

    WaitForRestoreFinished(&playlist);

    ASSERT_EQ(3, playlist.rowCount(QModelIndex()));
    EXPECT_EQ(u"Restored1"_s, TitleAt(playlist, 0));
    EXPECT_EQ(u"Restored2"_s, TitleAt(playlist, 1));
    EXPECT_EQ(u"Added1"_s, TitleAt(playlist, 2));

    EXPECT_FALSE(playlist.undo_stack()->canUndo());
    EXPECT_FALSE(playlist.undo_stack()->canRedo());
  }

  database->Close();

}

// Regression test: changes waiting for the save timer must be saved by SavePending() when exiting, since the timer would fire after the playlist backend exited.
TEST_F(PlaylistTest, SavePendingSavesScheduledChanges) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  SharedPtr<Database> database = std::make_shared<Database>(nullptr, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  SharedPtr<PlaylistBackend> playlist_backend = std::make_shared<PlaylistBackend>(database, tagreader_client_, nullptr);

  const int playlist_id = playlist_backend->CreatePlaylist(u"Pending"_s, QString());
  ASSERT_NE(-1, playlist_id);

  {
    Playlist playlist(nullptr, nullptr, playlist_backend, nullptr, tagreader_client_, playlist_id);
    WaitForRestoreFinished(&playlist);

    // Adding the item only starts the save timer.
    playlist.InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));
    ASSERT_TRUE(playlist_backend->GetPlaylistItems(playlist_id).isEmpty());

    // The save is queued to the playlist backend, process the events once, long before the save timer fires.
    playlist.SavePending();
    QCoreApplication::processEvents();

    const PlaylistItemPtrList items = playlist_backend->GetPlaylistItems(playlist_id);
    ASSERT_EQ(1, items.count());
    EXPECT_EQ(u"Added"_s, items.first()->OriginalMetadata().title());
  }

  database->Close();

}

// Regression test: changes made while the playlist is restored must not be saved before the restored items are inserted, saving the playlist replaces the saved items.
TEST_F(PlaylistTest, SavePendingWhileRestoringKeepsRestoredItems) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  SharedPtr<Database> database = std::make_shared<Database>(nullptr, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  SharedPtr<PlaylistBackend> playlist_backend = std::make_shared<PlaylistBackend>(database, tagreader_client_, nullptr);

  const int playlist_id = playlist_backend->CreatePlaylist(u"Restored"_s, QString());
  ASSERT_NE(-1, playlist_id);
  PlaylistItemSaveDataList items_save_data;
  items_save_data << MakeStreamItem(u"Restored1"_s)->CreateSaveData() << MakeStreamItem(u"Restored2"_s)->CreateSaveData();
  playlist_backend->SavePlaylist(playlist_id, items_save_data, -1, nullptr);

  {
    // The restore starts in the constructor, but the restored items are only inserted from the event loop.
    Playlist playlist(nullptr, nullptr, playlist_backend, nullptr, tagreader_client_, playlist_id);
    ASSERT_TRUE(playlist.is_restoring());

    playlist.InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));
    playlist.SavePending();

    WaitForRestoreFinished(&playlist);
    ASSERT_FALSE(playlist.is_restoring());
    ASSERT_EQ(3, playlist.rowCount(QModelIndex()));

    // The change is saved with the restored items, the save is queued to the playlist backend.
    playlist.SavePending();
    QCoreApplication::processEvents();

    const PlaylistItemPtrList items = playlist_backend->GetPlaylistItems(playlist_id);
    ASSERT_EQ(3, items.count());
    EXPECT_EQ(u"Restored1"_s, items.at(0)->OriginalMetadata().title());
    EXPECT_EQ(u"Restored2"_s, items.at(1)->OriginalMetadata().title());
    EXPECT_EQ(u"Added"_s, items.at(2)->OriginalMetadata().title());
  }

  database->Close();

}

// Regression test: removing the items that are not queued (used when repopulating a dynamic playlist) must clear the undo stack when every item is removed, since the items are removed without undo.
TEST_F(PlaylistTest, RemoveItemsNotInQueueClearsUndoStackWhenRemovingAll) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s));
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Three"_s));
  playlist_.undo_stack()->undo();
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  ASSERT_TRUE(playlist_.undo_stack()->canRedo());

  // Nothing is queued or playing, so every item is removed.
  CallRemoveItemsNotInQueue();

  EXPECT_EQ(0, playlist_.rowCount(QModelIndex()));
  EXPECT_FALSE(playlist_.undo_stack()->canUndo());
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());

}

// Regression test: the queue must only accept rows dragged from its own playlist, and skip rows that don't exist anymore.
TEST_F(PlaylistTest, QueueDropOnlyAcceptsExistingRowsOfOwnPlaylist) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s));

  // Rows dragged from another playlist are other songs, so they are not queued.
  Playlist other_playlist(nullptr, nullptr, nullptr, nullptr, tagreader_client_, 2);
  other_playlist.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"A"_s) << MakeMockItemP(u"B"_s) << MakeMockItemP(u"C"_s) << MakeMockItemP(u"D"_s) << MakeMockItemP(u"E"_s));
  {
    ScopedPtr<QMimeData> mimedata(other_playlist.mimeData(QModelIndexList() << other_playlist.index(4, 0)));
    ASSERT_TRUE(mimedata);
    EXPECT_FALSE(playlist_.queue()->dropMimeData(mimedata.get(), Qt::CopyAction, -1, 0, QModelIndex()));
    EXPECT_EQ(0, playlist_.queue()->rowCount(QModelIndex()));
  }

  // Data without the process ID is not accepted, even with the pointer of this playlist, it could come from another instance.
  {
    QByteArray rows_data;
    QDataStream stream(&rows_data, QIODevice::WriteOnly);
    const Playlist *playlist = &playlist_;
    stream.writeRawData(reinterpret_cast<const char*>(&playlist), sizeof(&playlist));
    stream << (QList<int>() << 0);
    QMimeData mimedata;
    mimedata.setData(QLatin1String(Playlist::kRowsMimetype), rows_data);
    EXPECT_FALSE(playlist_.queue()->dropMimeData(&mimedata, Qt::CopyAction, -1, 0, QModelIndex()));
    EXPECT_EQ(0, playlist_.queue()->rowCount(QModelIndex()));
  }

  // Rows of its own playlist that were removed during the drag are skipped, the others are queued.
  {
    ScopedPtr<QMimeData> mimedata(playlist_.mimeData(QModelIndexList() << playlist_.index(0, 0) << playlist_.index(2, 0)));
    ASSERT_TRUE(mimedata);
    ASSERT_TRUE(playlist_.removeRows(1, 2, QModelIndex()));
    ASSERT_EQ(1, playlist_.rowCount(QModelIndex()));
    EXPECT_TRUE(playlist_.queue()->dropMimeData(mimedata.get(), Qt::CopyAction, -1, 0, QModelIndex()));
    EXPECT_EQ(1, playlist_.queue()->rowCount(QModelIndex()));
    EXPECT_TRUE(playlist_.queue()->ContainsSourceRow(0));
  }

}

// Regression test: moving queued rows must put them before the row they are dropped on, counting from before they are taken out.
TEST_F(PlaylistTest, QueueMove) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"0"_s) << MakeMockItemP(u"1"_s) << MakeMockItemP(u"2"_s) << MakeMockItemP(u"3"_s) << MakeMockItemP(u"4"_s));

  Queue *queue = playlist_.queue();
  const auto queued_source_rows = [queue]() {
    QList<int> rows;
    for (int i = 0; i < queue->rowCount(QModelIndex()); ++i) {
      rows << queue->mapToSource(queue->index(i, 0)).row();
    }
    return rows;
  };
  const auto reset_queue = [this, queue]() {
    queue->Clear();
    QModelIndexList source_indexes;
    for (int i = 0; i < playlist_.rowCount(QModelIndex()); ++i) {
      source_indexes << playlist_.index(i, 0);
    }
    queue->ToggleTracks(source_indexes);
  };

  reset_queue();
  ASSERT_EQ(QList<int>() << 0 << 1 << 2 << 3 << 4, queued_source_rows());

  // Dropping rows right before the row after them changes nothing.
  queue->Move(QList<int>() << 0 << 1 << 2, 3);
  EXPECT_EQ(QList<int>() << 0 << 1 << 2 << 3 << 4, queued_source_rows());

  // Dropping a row on itself changes nothing.
  queue->Move(QList<int>() << 2, 2);
  EXPECT_EQ(QList<int>() << 0 << 1 << 2 << 3 << 4, queued_source_rows());

  queue->Move(QList<int>() << 0 << 2, 4);
  EXPECT_EQ(QList<int>() << 1 << 3 << 0 << 2 << 4, queued_source_rows());

  reset_queue();
  queue->Move(QList<int>() << 3 << 4, 1);
  EXPECT_EQ(QList<int>() << 0 << 3 << 4 << 1 << 2, queued_source_rows());

  reset_queue();
  queue->Move(QList<int>() << 1, -1);
  EXPECT_EQ(QList<int>() << 0 << 2 << 3 << 4 << 1, queued_source_rows());

  reset_queue();
  queue->MoveUp(2);
  EXPECT_EQ(QList<int>() << 0 << 2 << 1 << 3 << 4, queued_source_rows());

  reset_queue();
  queue->MoveDown(2);
  EXPECT_EQ(QList<int>() << 0 << 1 << 3 << 2 << 4, queued_source_rows());

}

// Regression test: the queue markers of each queued track must be updated, the queued tracks are not in order in the playlist, so one range from the first to the last could be reversed, and was ignored.
// The tracks after added or removed ones get a new queue position, so they must be updated too.
TEST_F(PlaylistTest, QueueMarkersUpdatedForEachQueuedTrack) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Zero"_s) << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));
  playlist_.queue()->ToggleTracks(QModelIndexList() << playlist_.index(4, 0) << playlist_.index(1, 0) << playlist_.index(5, 0));
  ASSERT_EQ(3, playlist_.queue()->rowCount(QModelIndex()));

  // Returns the rows updated by the spied dataChanged signals, and fails for reversed ranges.
  const auto updated_rows = [](const QSignalSpy &spy) {
    QSet<int> rows;
    for (const QList<QVariant> &arguments : spy) {
      const QModelIndex top_left = arguments.at(0).toModelIndex();
      const QModelIndex bottom_right = arguments.at(1).toModelIndex();
      EXPECT_LE(top_left.row(), bottom_right.row());
      for (int row = top_left.row(); row <= bottom_right.row(); ++row) {
        rows << row;
      }
    }
    return rows;
  };

  QSignalSpy spy(&playlist_, &Playlist::dataChanged);

  // Add two tracks to the front of the queue, in reverse order of the playlist, all queued tracks get a new position.
  playlist_.queue()->InsertFirst(QModelIndexList() << playlist_.index(3, 0) << playlist_.index(0, 0));
  ASSERT_EQ(5, playlist_.queue()->rowCount(QModelIndex()));
  EXPECT_EQ(QSet<int>() << 3 << 0 << 4 << 1 << 5, updated_rows(spy));

  // Remove a track from the middle of the queue, the removed track and the tracks after it are updated.
  spy.clear();
  playlist_.queue()->ToggleTracks(QModelIndexList() << playlist_.index(4, 0));
  ASSERT_EQ(4, playlist_.queue()->rowCount(QModelIndex()));
  EXPECT_EQ(QSet<int>() << 4 << 1 << 5, updated_rows(spy));

}

// Regression test: removing the items that are not queued must keep the current and queued items, and clear the undo stack.
TEST_F(PlaylistTest, RemoveItemsNotInQueueKeepsCurrentAndQueuedAndClearsUndoStack) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));
  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"Six"_s));
  playlist_.undo_stack()->undo();
  ASSERT_EQ(5, playlist_.rowCount(QModelIndex()));
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());
  ASSERT_TRUE(playlist_.undo_stack()->canRedo());

  playlist_.set_current_row(1);
  playlist_.queue()->ToggleTracks(QModelIndexList() << playlist_.index(3, 0));
  ASSERT_TRUE(playlist_.queue()->ContainsSourceRow(3));

  CallRemoveItemsNotInQueue();

  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"Two"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"Four"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(0, playlist_.current_row());
  EXPECT_TRUE(playlist_.queue()->ContainsSourceRow(1));

  EXPECT_FALSE(playlist_.undo_stack()->canUndo());
  EXPECT_FALSE(playlist_.undo_stack()->canRedo());

}

// Regression test: when every item is either current or queued, nothing is removed, so the undo stack must be kept.
TEST_F(PlaylistTest, RemoveItemsNotInQueueKeepsUndoStackWhenNothingRemoved) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s));
  ASSERT_TRUE(playlist_.undo_stack()->canUndo());

  playlist_.set_current_row(0);
  playlist_.queue()->ToggleTracks(QModelIndexList() << playlist_.index(1, 0));
  ASSERT_TRUE(playlist_.queue()->ContainsSourceRow(1));

  CallRemoveItemsNotInQueue();

  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_TRUE(playlist_.undo_stack()->canUndo());

}

// Removing scattered rows is one undo step, the removals of the consecutive rows are merged into one undo command.
TEST_F(PlaylistTest, ScatteredRemovalIsOneUndoStep) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"One"_s) << MakeMockItemP(u"Two"_s) << MakeMockItemP(u"Three"_s) << MakeMockItemP(u"Four"_s) << MakeMockItemP(u"Five"_s));
  const QPersistentModelIndex two_p(playlist_.index(1, 0));
  const int undo_count = playlist_.undo_stack()->count();

  QList<int> rows = QList<int>() << 0 << 2 << 4;
  ASSERT_TRUE(CallRemoveRows(rows));
  ASSERT_EQ(2, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(undo_count + 1, playlist_.undo_stack()->count());
  EXPECT_EQ(0, two_p.row());

  playlist_.undo_stack()->undo();
  ASSERT_EQ(5, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"One"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(u"Two"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"Three"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(u"Four"_s, TitleAt(playlist_, 3));
  EXPECT_EQ(u"Five"_s, TitleAt(playlist_, 4));
  EXPECT_EQ(1, two_p.row());

}

// Removing many rows at once, with and without undo, keeps the current row on the same item.
TEST_F(PlaylistTest, RemoveManyRowsKeepsCurrentItem) {

  PlaylistItemPtrList items;
  const int count = Playlist::kUndoItemLimit + 100;
  for (int i = 0; i < count; ++i) {
    items << MakeMockItemP(QString::number(i));
  }
  playlist_.InsertItems(items);
  playlist_.set_current_row(count - 10);

  // Too many rows to keep in the undo stack.
  ASSERT_TRUE(playlist_.removeRows(10, Playlist::kUndoItemLimit + 50));
  ASSERT_EQ(count - Playlist::kUndoItemLimit - 50, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"9"_s, TitleAt(playlist_, 9));
  EXPECT_EQ(QString::number(Playlist::kUndoItemLimit + 60), TitleAt(playlist_, 10));
  EXPECT_EQ(items[count - 10], playlist_.current_item());

  // Few enough rows to keep in the undo stack.
  const int rows_before = playlist_.rowCount(QModelIndex());
  ASSERT_TRUE(playlist_.removeRows(2, 5));
  ASSERT_EQ(rows_before - 5, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(items[count - 10], playlist_.current_item());
  playlist_.undo_stack()->undo();
  ASSERT_EQ(rows_before, playlist_.rowCount(QModelIndex()));
  EXPECT_EQ(u"2"_s, TitleAt(playlist_, 2));
  EXPECT_EQ(items[count - 10], playlist_.current_item());

}

// With shuffle, sorting and moving rows must keep the shuffled play order on the same items.
TEST_F(PlaylistTest, ReorderWithShuffleKeepsShuffleOrder) {

  playlist_.InsertItems(PlaylistItemPtrList() << MakeMockItemP(u"4"_s) << MakeMockItemP(u"3"_s) << MakeMockItemP(u"2"_s) << MakeMockItemP(u"1"_s) << MakeMockItemP(u"0"_s));
  playlist_.sequence()->SetShuffleMode(PlaylistSequence::ShuffleMode::All);
  SetVirtualOrder(QList<int>() << 3 << 0 << 4 << 1 << 2, -1);

  const auto shuffle_order_titles = [this]() {
    QStringList titles;
    const QList<int> virtual_order = VirtualOrder();
    for (const int row : virtual_order) titles << TitleAt(playlist_, row);
    return titles;
  };
  const QStringList expected_titles = QStringList() << u"1"_s << u"4"_s << u"0"_s << u"3"_s << u"2"_s;
  ASSERT_EQ(expected_titles, shuffle_order_titles());

  // Sort, which reorders all rows.
  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);
  ASSERT_EQ(u"0"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(expected_titles, shuffle_order_titles());

  // Move rows within the playlist, and undo the move.
  const QModelIndexList source_indexes = QModelIndexList() << playlist_.index(0, 0) << playlist_.index(2, 0);
  ScopedPtr<QMimeData> mimedata(playlist_.mimeData(source_indexes));
  ASSERT_TRUE(mimedata);
  ASSERT_TRUE(playlist_.dropMimeData(mimedata.get(), Qt::MoveAction, 5, 0, QModelIndex()));
  ASSERT_EQ(u"1"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(expected_titles, shuffle_order_titles());
  playlist_.undo_stack()->undo();
  ASSERT_EQ(u"0"_s, TitleAt(playlist_, 0));
  EXPECT_EQ(expected_titles, shuffle_order_titles());

}

// Updating the metadata of an item must update the row the item is at after rows were inserted, removed or reordered.
TEST_F(PlaylistTest, UpdateItemMetadataAfterRowsChanged) {

  const auto make_item = [](const QString &title) {
    Song song;
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl::fromLocalFile(u"/music/"_s + title + u".mp3"_s));
    return PlaylistItemPtr(std::make_shared<SongPlaylistItem>(song));
  };
  const auto changed_metadata = [](const PlaylistItemPtr &item, const QString &title) {
    Song song = item->OriginalMetadata();
    song.set_title(title);
    return song;
  };

  const PlaylistItemPtr item_a = make_item(u"A"_s);
  playlist_.InsertItems(PlaylistItemPtrList() << item_a << make_item(u"B"_s) << make_item(u"C"_s));

  playlist_.sort(static_cast<int>(Playlist::Column::Title), Qt::DescendingOrder);
  ASSERT_EQ(item_a, playlist_.item_at(2));
  EXPECT_TRUE(playlist_.UpdateItemMetadata(item_a, changed_metadata(item_a, u"A2"_s), false));
  EXPECT_EQ(u"A2"_s, TitleAt(playlist_, 2));

  playlist_.InsertItems(PlaylistItemPtrList() << make_item(u"D"_s), 0);
  ASSERT_EQ(item_a, playlist_.item_at(3));
  EXPECT_TRUE(playlist_.UpdateItemMetadata(item_a, changed_metadata(item_a, u"A3"_s), false));
  EXPECT_EQ(u"A3"_s, TitleAt(playlist_, 3));
  EXPECT_EQ(u"D"_s, TitleAt(playlist_, 0));

  ASSERT_TRUE(playlist_.removeRows(0, 2));
  ASSERT_EQ(item_a, playlist_.item_at(1));
  EXPECT_TRUE(playlist_.UpdateItemMetadata(item_a, changed_metadata(item_a, u"A4"_s), false));
  EXPECT_EQ(u"A4"_s, TitleAt(playlist_, 1));
  EXPECT_EQ(u"B"_s, TitleAt(playlist_, 0));

  // An item that is not in the playlist is not updated.
  const PlaylistItemPtr item_e = make_item(u"E"_s);
  EXPECT_FALSE(playlist_.UpdateItemMetadata(item_e, changed_metadata(item_e, u"E2"_s), false));
  // Neither is a null item.
  EXPECT_FALSE(playlist_.UpdateItemMetadata(PlaylistItemPtr(), changed_metadata(item_e, u"E3"_s), false));

}

}  // namespace
