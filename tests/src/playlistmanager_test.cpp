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

#include <memory>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/mimedata.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/urlhandlers.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentalbumcoverloader.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistcontainer.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "playlist/playlistfilter.h"
#include "playlist/playlistview.h"
#include "playlist/songloaderinserter.h"
#include "tagreader/tagreaderclient.h"
#include "mock_settingsprovider.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistManagerTest : public ::testing::Test {
 protected:
  PlaylistManagerTest()
      : tagreader_client_(new TagReaderClient()),
        tagreader_client_thread_(new QThread()),
        sequence_(nullptr, new DummySettingsProvider) {
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  ~PlaylistManagerTest() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  void SetUp() override {

    ASSERT_TRUE(temp_dir_.isValid());

    // A file database rather than a MemoryDatabase, since the playlists are restored from another thread, and each thread gets its own connection.
    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    playlist_backend_ = make_shared<PlaylistBackend>(database_, tagreader_client_, collection_backend_);
    current_albumcover_loader_ = make_shared<CurrentAlbumCoverLoader>(make_shared<AlbumCoverLoader>(tagreader_client_));

    playlist_manager_ = std::make_unique<PlaylistManager>(task_manager_, tagreader_client_, make_shared<UrlHandlers>(), playlist_backend_, collection_backend_, current_albumcover_loader_);
    playlist_manager_->Init(&sequence_, &playlist_container_);

  }

  void TearDown() override {
    playlist_manager_.reset();
    QCoreApplication::processEvents();
    database_->Close();
  }

  // Creates the playlist manager again, which restores the open playlists from the database like when starting.
  void RestartPlaylistManager() {
    playlist_manager_.reset();
    playlist_manager_ = std::make_unique<PlaylistManager>(task_manager_, tagreader_client_, make_shared<UrlHandlers>(), playlist_backend_, collection_backend_, current_albumcover_loader_);
    playlist_manager_->Init(&sequence_, &playlist_container_);
  }

  static void WaitForRestoreFinished(Playlist *playlist, const int timeout_ms = 5000) {
    if (!playlist->is_restoring()) return;
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

  // Waits for the open playlists to be restored, so saving the pending changes when exiting only waits for the playlists of the test.
  void WaitForOpenPlaylistsRestored() const {
    const QList<Playlist*> open_playlists = playlist_manager_->GetAllPlaylists();
    for (Playlist *open_playlist : open_playlists) {
      WaitForRestoreFinished(open_playlist);
    }
  }

  // Adds a favorite playlist which is still restoring, with an item added while restoring.
  int AddRestoringFavoritePlaylistWithItem() {
    playlist_manager_->New(u"Second"_s);
    const int id = playlist_manager_->current_id();
    Playlist *playlist = playlist_manager_->playlist(id);
    EXPECT_TRUE(playlist->is_restoring());
    playlist_manager_->Favorite(id, true);
    playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));
    return id;
  }

  // Creates a local file outside the collection, which is loaded asynchronously when it's added to a playlist.
  QUrl CreateLocalFile(const QString &filename) const {
    const QString path = temp_dir_.filePath(filename);
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("data");
    return QUrl::fromLocalFile(path);
  }

  static void DropUrl(Playlist *playlist, const QUrl &url, const bool clear_first) {
    MimeData mimedata(clear_first);
    mimedata.setUrls(QList<QUrl>() << url);
    playlist->dropMimeData(&mimedata, Qt::CopyAction, -1, 0, QModelIndex());
  }

  // Waits for the asynchronous loads to be done and for at least the given number of rows.
  void WaitForLoadedUrls(Playlist *playlist, const int rows) const {
    ASSERT_TRUE(QTest::qWaitFor([this, playlist, rows]() { return playlist->rowCount(QModelIndex()) >= rows && task_manager_->GetTasks().isEmpty(); }, 10000));
    QTest::qWait(200);
  }

  // Adds a stream, and emits the loaded metadata of the same stream from an inserter, with the load cancelled first if requested, returning the resulting title.
  QString TitleAfterLoadFinished(const bool cancel) {
    WaitForOpenPlaylistsRestored();
    Playlist *playlist = playlist_manager_->current();
    playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Stream"_s));
    SongLoaderInserter *inserter = new SongLoaderInserter(task_manager_, tagreader_client_, make_shared<UrlHandlers>(), collection_backend_);
    inserter->Load(playlist, -1, false, false, false, QList<QUrl>());
    if (cancel) Q_EMIT playlist->PendingInsertsCancelled();
    Song loaded_song = playlist->item_at(0)->EffectiveMetadata();
    loaded_song.set_title(u"Loaded"_s);
    Q_EMIT inserter->EffectiveLoadFinished(SongList() << loaded_song);
    return playlist->item_at(0)->EffectiveMetadata().title();
  }

  static PlaylistItemPtr MakeStreamItem(const QString &title) {
    Song song(Song::Source::Stream);
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl(u"http://example.com/"_s + title));
    return PlaylistItem::NewFromSong(song);
  }

  // Adds a second playlist, so it can be closed, and adds an item to it, which only starts the save timer.
  int AddPlaylistWithPendingItem(const bool favorite) {

    playlist_manager_->New(u"Second"_s);
    const int id = playlist_manager_->current_id();
    Playlist *playlist = playlist_manager_->playlist(id);
    WaitForRestoreFinished(playlist);
    if (favorite) playlist_manager_->Favorite(id, true);
    // Let the save of the new playlist, if any, finish, so only the added item is pending.
    playlist->SavePending();
    QCoreApplication::processEvents();

    playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));
    return id;

  }

  // Processes events until the playlist has items in the database, or the timeout.
  PlaylistItemPtrList WaitForSavedItems(const int id, const int timeout_ms = 5000) const {
    QElapsedTimer timer;
    timer.start();
    PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(id);
    while (items.isEmpty() && timer.elapsed() < timeout_ms) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      items = playlist_backend_->GetPlaylistItems(id);
    }
    return items;
  }

  QTemporaryDir temp_dir_;
  SharedPtr<TagReaderClient> tagreader_client_;
  QThread *tagreader_client_thread_;
  PlaylistSequence sequence_;
  PlaylistContainer playlist_container_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<PlaylistBackend> playlist_backend_;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  std::unique_ptr<PlaylistManager> playlist_manager_;
};

// Regression test: closing a favorite playlist within the save delay deleted the playlist with its save timer, so the last changes were lost.
TEST_F(PlaylistManagerTest, CloseFavoritePlaylistSavesPendingChanges) {

  const int id = AddPlaylistWithPendingItem(true);
  if (HasFatalFailure()) return;
  ASSERT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());

  ASSERT_TRUE(playlist_manager_->Close(id));
  // The save is queued to the playlist backend, long before the save timer would have fired.
  QCoreApplication::processEvents();

  EXPECT_EQ(id, playlist_backend_->GetPlaylist(id).id);
  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(id);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"Added"_s, items.first()->OriginalMetadata().title());

}

// Deleting a favorite playlist doesn't save its pending changes after the playlist is removed.
TEST_F(PlaylistManagerTest, DeleteFavoritePlaylistDoesNotSavePendingChanges) {

  const int id = AddPlaylistWithPendingItem(true);
  if (HasFatalFailure()) return;

  playlist_manager_->Delete(id);
  QCoreApplication::processEvents();

  EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());

}

// Closing a playlist which is not a favorite removes it, its pending changes are not saved.
TEST_F(PlaylistManagerTest, CloseOtherPlaylistDoesNotSavePendingChanges) {

  const int id = AddPlaylistWithPendingItem(false);
  if (HasFatalFailure()) return;

  ASSERT_TRUE(playlist_manager_->Close(id));
  QCoreApplication::processEvents();

  EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());

}

// Regression test: changes made while a favorite playlist is restored are only saved when it's restored, closing it before that deleted them.
TEST_F(PlaylistManagerTest, CloseFavoritePlaylistWhileRestoringSavesPendingChanges) {

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  Playlist *playlist = playlist_manager_->playlist(id);
  // The restore started in the constructor only finishes from the event loop.
  ASSERT_TRUE(playlist->is_restoring());
  playlist_manager_->Favorite(id, true);
  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));

  ASSERT_TRUE(playlist_manager_->Close(id));

  const PlaylistItemPtrList items = WaitForSavedItems(id);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"Added"_s, items.first()->OriginalMetadata().title());
  EXPECT_EQ(id, playlist_backend_->GetPlaylist(id).id);

}

// Regression test: when a closed favorite playlist was removed from the favorites before its pending changes were saved, the save added its items again, for a playlist which doesn't exist.
TEST_F(PlaylistManagerTest, ClosedFavoritePlaylistRemovedBeforeSaveDoesNotSaveItems) {

  const int id = AddPlaylistWithPendingItem(true);
  if (HasFatalFailure()) return;

  // The save is queued to the playlist backend, the removal is not.
  ASSERT_TRUE(playlist_manager_->Close(id));
  playlist_manager_->Favorite(id, false);
  QCoreApplication::processEvents();

  EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());

}

// Regression test: deleting a playlist which is not a favorite removed it twice, and emitted PlaylistDeleted() twice.
TEST_F(PlaylistManagerTest, DeletePlaylistEmitsPlaylistDeletedOnce) {

  for (const bool favorite : {false, true}) {
    SCOPED_TRACE(favorite);
    const int id = AddPlaylistWithPendingItem(favorite);
    if (HasFatalFailure()) return;

    QSignalSpy spy(&*playlist_manager_, &PlaylistManager::PlaylistDeleted);
    playlist_manager_->Delete(id);
    QCoreApplication::processEvents();

    EXPECT_EQ(1, spy.count());
    EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
    EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());
  }

}

// Regression test: opening a closed favorite playlist before its pending changes were saved restored it from the database without them.
TEST_F(PlaylistManagerTest, ReopenClosedFavoritePlaylistBeforeSaveKeepsChanges) {

  const int id = AddPlaylistWithPendingItem(true);
  if (HasFatalFailure()) return;
  const QPointer<Playlist> playlist = playlist_manager_->playlist(id);

  ASSERT_TRUE(playlist_manager_->Close(id));
  playlist_manager_->Open(id);

  // The closed playlist is used again, and isn't deleted when its save is done.
  EXPECT_EQ(playlist.data(), playlist_manager_->playlist(id));
  QCoreApplication::processEvents();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  ASSERT_FALSE(playlist.isNull());
  EXPECT_EQ(1, playlist->rowCount(QModelIndex()));
  EXPECT_EQ(1, WaitForSavedItems(id).count());

}

// Regression test: opening a closed favorite playlist which was still restoring created another playlist, without the changes made while restoring.
TEST_F(PlaylistManagerTest, ReopenClosedFavoritePlaylistWhileRestoringKeepsChanges) {

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  const QPointer<Playlist> playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist->is_restoring());
  playlist_manager_->Favorite(id, true);
  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));

  ASSERT_TRUE(playlist_manager_->Close(id));
  playlist_manager_->Open(id);
  EXPECT_EQ(playlist.data(), playlist_manager_->playlist(id));

  WaitForRestoreFinished(playlist);
  ASSERT_FALSE(playlist.isNull());
  EXPECT_EQ(1, playlist->rowCount(QModelIndex()));
  const PlaylistItemPtrList items = WaitForSavedItems(id);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"Added"_s, items.first()->OriginalMetadata().title());

}

// Regression test: when a closed favorite playlist was removed before its queued save ran, a new playlist could get the same id, and the save replaced its items.
TEST_F(PlaylistManagerTest, NewPlaylistAfterRemovingClosedFavoritePlaylistKeepsItsItems) {

  const int id = AddPlaylistWithPendingItem(true);
  if (HasFatalFailure()) return;

  ASSERT_TRUE(playlist_manager_->Close(id));
  playlist_manager_->Favorite(id, false);
  playlist_manager_->New(u"Third"_s);
  const int new_id = playlist_manager_->current_id();
  QCoreApplication::processEvents();

  EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());
  EXPECT_EQ(new_id, playlist_backend_->GetPlaylist(new_id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(new_id).isEmpty());

}

// Regression test: saving the pending changes when exiting didn't wait for a closed favorite playlist which was still restoring.
TEST_F(PlaylistManagerTest, SavePendingPlaylistsWaitsForClosedRestoringPlaylist) {

  // The open playlists are restored, so only the closed playlist is waited for.
  const QList<Playlist*> open_playlists = playlist_manager_->GetAllPlaylists();
  for (Playlist *open_playlist : open_playlists) {
    WaitForRestoreFinished(open_playlist);
  }

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  Playlist *playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist->is_restoring());
  playlist_manager_->Favorite(id, true);
  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added"_s));
  ASSERT_TRUE(playlist_manager_->Close(id));

  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::PendingPlaylistsSaved);
  playlist_manager_->SavePendingPlaylistsAsync();
  EXPECT_EQ(0, spy.count());

  ASSERT_TRUE(spy.wait(5000));
  // The save is queued before the playlist backend exits.
  QCoreApplication::processEvents();
  const PlaylistItemPtrList items = playlist_backend_->GetPlaylistItems(id);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"Added"_s, items.first()->OriginalMetadata().title());

}

// Regression test: closing a restoring favorite playlist after saving the pending changes when exiting started disconnected the exit handling, so it never finished.
TEST_F(PlaylistManagerTest, CloseRestoringFavoritePlaylistWhileExitingSavesIt) {

  WaitForOpenPlaylistsRestored();
  const int id = AddRestoringFavoritePlaylistWithItem();

  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::PendingPlaylistsSaved);
  playlist_manager_->SavePendingPlaylistsAsync();
  EXPECT_EQ(0, spy.count());
  ASSERT_TRUE(playlist_manager_->Close(id));

  ASSERT_TRUE(spy.wait(5000));
  QCoreApplication::processEvents();
  EXPECT_EQ(1, playlist_backend_->GetPlaylistItems(id).count());

}

// Regression test: opening a closed restoring playlist again while exiting disconnected the exit handling, and it wasn't saved.
TEST_F(PlaylistManagerTest, ReopenClosedRestoringPlaylistWhileExitingSavesIt) {

  WaitForOpenPlaylistsRestored();
  const int id = AddRestoringFavoritePlaylistWithItem();
  ASSERT_TRUE(playlist_manager_->Close(id));

  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::PendingPlaylistsSaved);
  playlist_manager_->SavePendingPlaylistsAsync();
  playlist_manager_->Open(id);

  ASSERT_TRUE(spy.wait(5000));
  QCoreApplication::processEvents();
  EXPECT_EQ(1, playlist_backend_->GetPlaylistItems(id).count());

}

// Regression test: removing a closed restoring playlist while exiting disconnected the exit handling, so it never finished.
TEST_F(PlaylistManagerTest, RemoveClosedRestoringPlaylistWhileExitingFinishes) {

  WaitForOpenPlaylistsRestored();
  const int id = AddRestoringFavoritePlaylistWithItem();
  // The pointer is taken before closing, playlist() only returns the open playlists.
  const QPointer<Playlist> playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist_manager_->Close(id));
  ASSERT_FALSE(playlist.isNull());

  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::PendingPlaylistsSaved);
  playlist_manager_->SavePendingPlaylistsAsync();
  playlist_manager_->Favorite(id, false);

  // The playlist is deleted right away, so its restore can't save it after it was removed.
  EXPECT_TRUE(playlist.isNull());
  EXPECT_EQ(1, spy.count());
  // The removal is queued before the exit is reported.
  QCoreApplication::processEvents();
  EXPECT_EQ(-1, playlist_backend_->GetPlaylist(id).id);
  EXPECT_TRUE(playlist_backend_->GetPlaylistItems(id).isEmpty());

}

// Regression test: the view didn't disconnect RestoreFinished of the previous playlist, so a closed playlist being restored made the view jump to the last played song of the playlist shown.
TEST_F(PlaylistManagerTest, ViewIgnoresRestoreFinishedOfPreviousPlaylist) {

  WaitForOpenPlaylistsRestored();
  Playlist *first_playlist = playlist_manager_->current();
  first_playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"First"_s) << MakeStreamItem(u"Second"_s));
  first_playlist->set_current_row(0);
  ASSERT_EQ(0, first_playlist->last_played_row());

  playlist_manager_->New(u"Second"_s);
  Playlist *second_playlist = playlist_manager_->current();
  ASSERT_NE(first_playlist, second_playlist);

  // Like PlaylistContainer::SetViewModel() does when the current playlist changes.
  PlaylistView *view = playlist_container_.view();
  view->setModel(second_playlist->filter());
  view->SetPlaylist(second_playlist);
  view->setModel(first_playlist->filter());
  view->SetPlaylist(first_playlist);

  // The user selects another song.
  const QModelIndex selected_index = first_playlist->filter()->mapFromSource(first_playlist->index(1, 0));
  view->setCurrentIndex(selected_index);

  Q_EMIT second_playlist->RestoreFinished();
  EXPECT_EQ(selected_index, view->currentIndex());

}

// Regression test: a closed playlist moved to another folder before it was saved kept the folder it was in when it was opened again.
TEST_F(PlaylistManagerTest, ReopenClosedPlaylistMovedToAnotherFolderUsesNewFolder) {

  WaitForOpenPlaylistsRestored();
  const int id = AddRestoringFavoritePlaylistWithItem();
  Playlist *playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist_manager_->Close(id));

  // Like PlaylistListContainer::PlaylistPathChanged() does for a closed playlist.
  playlist_backend_->SetPlaylistUiPath(id, u"Folder"_s);

  playlist_manager_->Open(id);
  ASSERT_EQ(playlist, playlist_manager_->playlist(id));
  EXPECT_EQ(u"Folder"_s, playlist->ui_path());

}

// Regression test: the closed playlists which were kept until they were saved were not updated when songs changed in the collection, so they had the old metadata when they were opened again.
TEST_F(PlaylistManagerTest, ReopenClosedPlaylistHasCollectionChanges) {

  WaitForOpenPlaylistsRestored();
  const int id = AddRestoringFavoritePlaylistWithItem();
  Playlist *playlist = playlist_manager_->playlist(id);

  Song song(Song::Source::Collection);
  song.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
  song.set_id(1);
  song.set_directory_id(1);
  song.set_url(QUrl::fromLocalFile(u"/music/title.flac"_s));
  playlist->InsertItems(PlaylistItemPtrList() << PlaylistItem::NewFromSong(song));
  ASSERT_TRUE(playlist_manager_->Close(id));

  song.set_title(u"Changed title"_s);
  Q_EMIT collection_backend_->SongsChanged(SongList() << song);

  playlist_manager_->Open(id);
  ASSERT_EQ(playlist, playlist_manager_->playlist(id));
  const PlaylistItemPtrList items = playlist->collection_items(Song::Source::Collection, 1);
  ASSERT_EQ(1, items.count());
  EXPECT_EQ(u"Changed title"_s, items.first()->EffectiveMetadata().title());

}

// Regression test: changes made after the save of a closed playlist was queued were lost, since the playlist was deleted when that save was done.
TEST_F(PlaylistManagerTest, ClosedPlaylistChangedAfterCloseIsSaved) {

  const int id = AddPlaylistWithPendingItem(true);
  const QPointer<Playlist> playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist_manager_->Close(id));
  ASSERT_FALSE(playlist.isNull());

  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added later"_s));

  EXPECT_TRUE(QTest::qWaitFor([this, id]() { return playlist_backend_->GetPlaylistItems(id).count() == 2; }, 5000));
  EXPECT_TRUE(QTest::qWaitFor([&playlist]() { return playlist.isNull(); }, 5000));

}

// Regression test: changes made to a closed playlist after its save was queued were not saved when exiting.
TEST_F(PlaylistManagerTest, SavePendingPlaylistsSavesChangedClosedPlaylist) {

  WaitForOpenPlaylistsRestored();
  const int id = AddPlaylistWithPendingItem(true);
  Playlist *playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist_manager_->Close(id));

  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added later"_s));
  ASSERT_TRUE(playlist->has_pending_save());

  playlist_manager_->SavePendingPlaylistsAsync();
  EXPECT_FALSE(playlist->has_pending_save());

}

// Regression test: songs could still be dragged from a closed favorite playlist, since it's kept until it's saved.
TEST_F(PlaylistManagerTest, DropAfterFavoritePlaylistClosedIsIgnoredUntilOpenedAgain) {

  WaitForOpenPlaylistsRestored();
  Playlist *destination_playlist = playlist_manager_->current();
  const int id = AddPlaylistWithPendingItem(true);
  Playlist *playlist = playlist_manager_->playlist(id);
  ScopedPtr<QMimeData> mimedata(playlist->mimeData(QModelIndexList() << playlist->index(0, 0)));
  ASSERT_TRUE(mimedata);

  ASSERT_TRUE(playlist_manager_->Close(id));
  destination_playlist->dropMimeData(mimedata.get(), Qt::CopyAction, -1, 0, QModelIndex());
  EXPECT_EQ(0, destination_playlist->rowCount(QModelIndex()));

  // The playlist is the same one when it's opened again, so the drag is valid again.
  playlist_manager_->Open(id);
  ASSERT_EQ(playlist, playlist_manager_->playlist(id));
  destination_playlist->dropMimeData(mimedata.get(), Qt::CopyAction, -1, 0, QModelIndex());
  EXPECT_EQ(1, destination_playlist->rowCount(QModelIndex()));

}

// Regression test: a closed playlist was deleted when a save was done, also when a newer save was queued behind it, so opening it again could read the old items from the database.
TEST_F(PlaylistManagerTest, ClosedPlaylistIsKeptUntilNewestSaveIsDone) {

  const int id = AddPlaylistWithPendingItem(true);
  Playlist *playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist_manager_->Close(id));

  // The save timer fires before the notification of the first save is handled.
  playlist->InsertItems(PlaylistItemPtrList() << MakeStreamItem(u"Added later"_s));
  playlist->SavePending();
  ASSERT_FALSE(playlist->has_pending_save());

  // Opened right after the playlist manager handled the notification of the first save.
  Playlist *opened_playlist = nullptr;
  QObject::connect(&*playlist_backend_, &PlaylistBackend::PlaylistSaved, &*playlist_manager_, [this, id, &opened_playlist]() {
    playlist_manager_->Open(id);
    opened_playlist = playlist_manager_->playlist(id);
  }, Qt::SingleShotConnection);

  ASSERT_TRUE(QTest::qWaitFor([&opened_playlist]() { return opened_playlist != nullptr; }, 5000));
  EXPECT_EQ(playlist, opened_playlist);

}

// Regression test: the playlist created on the first run was not counted, so it was never reported that the playlists were loaded.
TEST_F(PlaylistManagerTest, AllPlaylistsLoadedIsSentOnFirstRun) {

  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::AllPlaylistsLoaded);
  ASSERT_EQ(1, playlist_manager_->GetAllPlaylists().count());
  EXPECT_EQ(0, spy.count());

  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(1, spy.count());

}

// Regression test: a playlist cleared while restoring never reported that it was loaded.
TEST_F(PlaylistManagerTest, AllPlaylistsLoadedIsSentWhenRestoringPlaylistIsCleared) {

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  RestartPlaylistManager();
  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::AllPlaylistsLoaded);

  Playlist *playlist = playlist_manager_->playlist(id);
  ASSERT_TRUE(playlist->is_restoring());
  playlist->Clear();

  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(1, spy.count());

}

// Regression test: a playlist closed while restoring was deleted without reporting that it was loaded.
TEST_F(PlaylistManagerTest, AllPlaylistsLoadedIsSentWhenRestoringPlaylistIsClosed) {

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  RestartPlaylistManager();
  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::AllPlaylistsLoaded);

  ASSERT_TRUE(playlist_manager_->playlist(id)->is_restoring());
  ASSERT_TRUE(playlist_manager_->Close(id));

  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(1, spy.count());

}

// Regression test: a closed favorite playlist removed while restoring was deleted without reporting that it was loaded.
TEST_F(PlaylistManagerTest, AllPlaylistsLoadedIsSentWhenClosedRestoringPlaylistIsRemoved) {

  playlist_manager_->New(u"Second"_s);
  const int id = playlist_manager_->current_id();
  playlist_manager_->Favorite(id, true);
  RestartPlaylistManager();
  QSignalSpy spy(&*playlist_manager_, &PlaylistManager::AllPlaylistsLoaded);

  ASSERT_TRUE(playlist_manager_->playlist(id)->is_restoring());
  ASSERT_TRUE(playlist_manager_->Close(id));
  playlist_manager_->Favorite(id, false);

  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(1, spy.count());

}

// Regression test: URLs still being loaded when the playlist was replaced were inserted together with the new items.
TEST_F(PlaylistManagerTest, ReplacingPlaylistCancelsUrlsBeingLoaded) {

  WaitForOpenPlaylistsRestored();
  Playlist *playlist = playlist_manager_->current();
  const QUrl first_url = CreateLocalFile(u"first.mp3"_s);
  const QUrl second_url = CreateLocalFile(u"second.mp3"_s);

  DropUrl(playlist, first_url, true);
  DropUrl(playlist, second_url, true);

  WaitForLoadedUrls(playlist, 1);
  ASSERT_EQ(1, playlist->rowCount(QModelIndex()));
  EXPECT_EQ(second_url, playlist->item_at(0)->OriginalMetadata().url());

}

// Adding URLs without replacing the playlist doesn't cancel the URLs being loaded.
TEST_F(PlaylistManagerTest, AppendingKeepsUrlsBeingLoaded) {

  WaitForOpenPlaylistsRestored();
  Playlist *playlist = playlist_manager_->current();

  DropUrl(playlist, CreateLocalFile(u"first.mp3"_s), false);
  DropUrl(playlist, CreateLocalFile(u"second.mp3"_s), false);

  WaitForLoadedUrls(playlist, 2);
  EXPECT_EQ(2, playlist->rowCount(QModelIndex()));

}

// Regression test: a cancelled load updated the items of the new load with the same URL.
TEST_F(PlaylistManagerTest, CancelledLoadDoesNotUpdateItems) {

  EXPECT_EQ(u"Stream"_s, TitleAfterLoadFinished(true));

}

// A load which isn't cancelled updates the items.
TEST_F(PlaylistManagerTest, LoadUpdatesItems) {

  EXPECT_EQ(u"Loaded"_s, TitleAfterLoadFinished(false));

}

}  // namespace
