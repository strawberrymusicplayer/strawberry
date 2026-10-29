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

#include "config.h"

#include <memory>

#include "gtest_include.h"

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QAction>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QColor>
#include <QList>
#include <QListWidgetItem>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverimageresult.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/albumcovermanager.h"
#include "streaming/streamingservices.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

class AlbumCoverManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tagreader_client_ = make_shared<TagReaderClient>();
    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<MemoryDatabase>(nullptr);
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    albumcover_loader_ = make_shared<AlbumCoverLoader>(tagreader_client_);
    cover_manager_ = std::make_unique<AlbumCoverManager>(make_shared<NetworkAccessManager>(), collection_backend_, tagreader_client_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>(), &mainwindow_);
    cover_manager_->Init();
    // The default cover types.
    SetCoverTypes(AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Unset << AlbumCoverLoaderOptions::Type::Embedded << AlbumCoverLoaderOptions::Type::Manual << AlbumCoverLoaderOptions::Type::Automatic);
  }

  void TearDown() override {
    cover_manager_.reset();
  }

  // Adds an album which shows no cover, like before the thumbnail is loaded.
  AlbumItem *AddAlbum() {
    QListWidget *albums = cover_manager_->findChild<QListWidget*>(u"albums"_s);
    AlbumItem *album_item = new AlbumItem(cover_manager_->icon_nocover_item_, u"Artist - Album"_s, albums);
    album_item->urls << QUrl::fromLocalFile(u"/music/song.flac"_s);
    return album_item;
  }

  bool ItemHasCover(const AlbumItem *album_item) const { return cover_manager_->ItemHasCover(*album_item); }

  void SetCoverTypes(const AlbumCoverLoaderOptions::Types &cover_types) { cover_manager_->cover_types_ = cover_types; }

  // Queues loading the thumbnail, and adds a running thumbnail load, without loading them.
  void AddThumbnailLoads(AlbumItem *pending_album_item, AlbumItem *loading_album_item) {
    cover_manager_->cover_loading_pending_.enqueue(pending_album_item);
    cover_manager_->cover_loading_tasks_.insert(1, loading_album_item);
  }

  qsizetype PendingThumbnailLoads() const { return cover_manager_->cover_loading_pending_.count(); }
  qsizetype RunningThumbnailLoads() const { return cover_manager_->cover_loading_tasks_.count(); }

  void ClickAbort() { cover_manager_->abort_progress_->click(); }

  void StartThumbnailLoad(const quint64 id, AlbumItem *album_item) { cover_manager_->cover_loading_tasks_.insert(id, album_item); }

  // Delivers a successful thumbnail result for the load.
  void ThumbnailLoaded(const quint64 id) {
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::red);
    cover_manager_->AlbumCoverLoaded(id, AlbumCoverLoaderResult(true, AlbumCoverLoaderResult::Type::Manual, AlbumCoverImageResult(), image));
  }

  void SetContextMenuItems(const QList<QListWidgetItem*> &items) { cover_manager_->context_menu_items_ = items; }
  void UnsetCover() { cover_manager_->UnsetCover(); }
  void ClearCover() { cover_manager_->ClearCover(); }
  void DeleteCover() { cover_manager_->DeleteCover(); }

  void ShowAlbumsWithCovers() { cover_manager_->filter_with_covers_->setChecked(true); }
  void UpdateFilter() { cover_manager_->UpdateFilter(); }
  QString AlbumsWithoutCover() const { return cover_manager_->findChild<QLabel*>(u"without_cover"_s)->text(); }

  static void SetCoverLoaded(AlbumItem *album_item) { album_item->setData(AlbumCoverManager::Role_CoverLoaded, true); }

  static constexpr int kRoleArtEmbedded = AlbumCoverManager::Role_ArtEmbedded;
  static constexpr int kRoleArtAutomatic = AlbumCoverManager::Role_ArtAutomatic;
  static constexpr int kRoleArtManual = AlbumCoverManager::Role_ArtManual;
  static constexpr int kRoleArtUnset = AlbumCoverManager::Role_ArtUnset;

  QMainWindow mainwindow_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  std::unique_ptr<AlbumCoverManager> cover_manager_;
};

namespace {

// Regression test: albums were handled as albums without covers until their thumbnails were loaded, so fetching the missing covers right after opening the cover manager replaced the existing covers.
TEST_F(AlbumCoverManagerTest, AlbumWithCoverHasCoverBeforeThumbnailIsLoaded) {

  AlbumItem *manual = AddAlbum();
  manual->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  EXPECT_TRUE(ItemHasCover(manual));

  AlbumItem *automatic = AddAlbum();
  automatic->setData(kRoleArtAutomatic, QUrl::fromLocalFile(u"/music/cover.jpg"_s));
  EXPECT_TRUE(ItemHasCover(automatic));

  AlbumItem *embedded = AddAlbum();
  embedded->setData(kRoleArtEmbedded, true);
  EXPECT_TRUE(ItemHasCover(embedded));

}

TEST_F(AlbumCoverManagerTest, AlbumWithoutCoverHasNoCoverBeforeThumbnailIsLoaded) {

  EXPECT_FALSE(ItemHasCover(AddAlbum()));

  AlbumItem *unset = AddAlbum();
  unset->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  unset->setData(kRoleArtUnset, true);
  EXPECT_FALSE(ItemHasCover(unset));

}

// Only the cover types shown in the cover manager are used.
TEST_F(AlbumCoverManagerTest, CoverTypeNotShownIsNotUsedBeforeThumbnailIsLoaded) {

  SetCoverTypes(AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual);

  AlbumItem *embedded = AddAlbum();
  embedded->setData(kRoleArtEmbedded, true);
  EXPECT_FALSE(ItemHasCover(embedded));

}

// A cover which couldn't be loaded, like a deleted cover file, is missing, so it's fetched.
TEST_F(AlbumCoverManagerTest, CoverWhichCouldNotBeLoadedIsMissing) {

  AlbumItem *album_item = AddAlbum();
  album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/deleted.jpg"_s));
  SetCoverLoaded(album_item);
  EXPECT_FALSE(ItemHasCover(album_item));

}

// An unset album is only handled as an album without a cover when the unset type is used before the other types, like when the thumbnail is loaded.
TEST_F(AlbumCoverManagerTest, UnsetAfterOtherCoverTypesUsesCoverBeforeThumbnailIsLoaded) {

  SetCoverTypes(AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual << AlbumCoverLoaderOptions::Type::Unset);

  AlbumItem *album_item = AddAlbum();
  album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  album_item->setData(kRoleArtUnset, true);
  EXPECT_TRUE(ItemHasCover(album_item));

  // The unset type is not used.
  SetCoverTypes(AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual);
  EXPECT_TRUE(ItemHasCover(album_item));

}

// Regression test: aborting fetching the covers also stopped loading the thumbnails, then albums with covers which couldn't be loaded were not handled as missing until the albums were reloaded.
TEST_F(AlbumCoverManagerTest, AbortKeepsLoadingThumbnails) {

  AddThumbnailLoads(AddAlbum(), AddAlbum());
  ClickAbort();
  EXPECT_EQ(1, PendingThumbnailLoads());
  EXPECT_EQ(1, RunningThumbnailLoads());

}

// Regression test: the thumbnail of a cover which was removed while it was loading was shown when it was loaded, and the album was handled as an album with a cover.
TEST_F(AlbumCoverManagerTest, ThumbnailLoadedAfterRemovingCoverIsIgnored) {

  AlbumItem *unset = AddAlbum();
  AlbumItem *clear = AddAlbum();
  AlbumItem *deleted = AddAlbum();
  quint64 id = 0;
  for (AlbumItem *album_item : {unset, clear, deleted}) {
    album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
    StartThumbnailLoad(++id, album_item);
  }

  SetContextMenuItems(QList<QListWidgetItem*>() << unset);
  UnsetCover();
  SetContextMenuItems(QList<QListWidgetItem*>() << clear);
  ClearCover();
  SetContextMenuItems(QList<QListWidgetItem*>() << deleted);
  DeleteCover();
  EXPECT_EQ(0, RunningThumbnailLoads());

  for (quint64 loaded_id = 1; loaded_id <= id; ++loaded_id) {
    ThumbnailLoaded(loaded_id);
  }
  EXPECT_FALSE(ItemHasCover(unset));
  EXPECT_FALSE(ItemHasCover(clear));
  EXPECT_FALSE(ItemHasCover(deleted));

}

// Regression test: removing a cover while its thumbnail was loading didn't update the albums shown and the number of albums without covers, since the thumbnail load which updated them was canceled.
TEST_F(AlbumCoverManagerTest, RemovingCoverWhileThumbnailIsLoadingUpdatesFilter) {

  AlbumItem *unset = AddAlbum();
  AlbumItem *clear = AddAlbum();
  AlbumItem *deleted = AddAlbum();
  quint64 id = 0;
  for (AlbumItem *album_item : {unset, clear, deleted}) {
    album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
    StartThumbnailLoad(++id, album_item);
  }
  ShowAlbumsWithCovers();
  UpdateFilter();
  EXPECT_EQ(u"0"_s, AlbumsWithoutCover());

  SetContextMenuItems(QList<QListWidgetItem*>() << unset);
  UnsetCover();
  EXPECT_TRUE(unset->isHidden());
  EXPECT_FALSE(clear->isHidden());

  SetContextMenuItems(QList<QListWidgetItem*>() << clear);
  ClearCover();
  EXPECT_TRUE(clear->isHidden());
  EXPECT_FALSE(deleted->isHidden());

  SetContextMenuItems(QList<QListWidgetItem*>() << deleted);
  DeleteCover();
  EXPECT_TRUE(deleted->isHidden());

}

// The number of albums without covers is updated when all albums are shown.
TEST_F(AlbumCoverManagerTest, RemovingCoverWhileThumbnailIsLoadingUpdatesAlbumsWithoutCover) {

  AlbumItem *album_item = AddAlbum();
  album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  StartThumbnailLoad(1, album_item);
  UpdateFilter();
  EXPECT_EQ(u"0"_s, AlbumsWithoutCover());

  SetContextMenuItems(QList<QListWidgetItem*>() << album_item);
  UnsetCover();
  EXPECT_EQ(u"1"_s, AlbumsWithoutCover());

}

}  // namespace
