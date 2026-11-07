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
#include <QStringList>
#include <QUrl>
#include <QFile>
#include <QBuffer>
#include <QByteArray>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariant>
#include <QImage>
#include <QColor>
#include <QList>
#include <QListWidgetItem>
#include <QSignalSpy>
#include <QThread>
#include <QTest>

#include "test_utils.h"

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreaderreply.h"
#include "tagreader/tagreaderresult.h"
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
    // The tag reader runs on its own thread like in the application, saving covers asynchronously requires it.
    tagreader_client_ = SharedPtr<TagReaderClient>(new TagReaderClient(), [](TagReaderClient *tagreader_client) { tagreader_client->deleteLater(); });
    tagreader_client_thread_ = new QThread();
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
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
    // Release the tag reader before stopping its thread, it's deleted with deleteLater() on its thread.
    albumcover_loader_.reset();
    tagreader_client_.reset();
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
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

  // Starts a save of an embedded cover for the album, like when saving the cover to the files of the album.
  void StartEmbeddedCoverSave(AlbumItem *album_item, const QUrl &url) { cover_manager_->cover_save_tasks_.insert(album_item, url); }

  // Finishes a save of an embedded cover started for the album, like the tag reader does when the cover was saved or saving it failed.
  void FinishEmbeddedCoverSave(AlbumItem *album_item, const QUrl &url, const bool success) {
    TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(url.toLocalFile());
    reply->set_result(TagReaderResult(success ? TagReaderResult::ErrorCode::Success : TagReaderResult::ErrorCode::FileOpenError));
    cover_manager_->SaveEmbeddedCoverFinished(reply, album_item, url, true);
  }
  void SetSaveEmbeddedCover() { cover_manager_->album_cover_choice_controller_->set_save_embedded_cover_override(true); }
  void SaveImageToAlbums(const AlbumCoverImageResult &result) {
    Song song = cover_manager_->GetFirstSelectedAsSong();
    cover_manager_->SaveImageToAlbums(&song, result);
  }
  bool CoverIsBeingEmbedded(AlbumItem *album_item) const { return cover_manager_->cover_save_tasks_.contains(album_item); }
  static void SetAlbum(AlbumItem *album_item, const QString &album, const Song::FileType filetype, const QUrl &url) {
    album_item->setData(AlbumCoverManager::Role_AlbumArtist, u"Artist"_s);
    album_item->setData(AlbumCoverManager::Role_Album, album);
    album_item->setData(AlbumCoverManager::Role_Filetype, QVariant::fromValue(filetype));
    album_item->urls = QList<QUrl>() << url;
  }
  void UnsetCover() { cover_manager_->UnsetCover(); }
  void ClearCover() { cover_manager_->ClearCover(); }
  void DeleteCover() { cover_manager_->DeleteCover(); }

  void ShowAlbumsWithCovers() { cover_manager_->filter_with_covers_->setChecked(true); }
  void UpdateFilter() { cover_manager_->UpdateFilter(); }
  QString AlbumsWithoutCover() const { return cover_manager_->findChild<QLabel*>(u"without_cover"_s)->text(); }

  // Returns a cover with the image data set, like the covers loaded from a file, downloaded or found by searching, the image data is what's embedded.
  static AlbumCoverImageResult JpegCover() {
    AlbumCoverImageResult result;
    result.image = QImage(16, 16, QImage::Format_RGB32);
    result.image.fill(Qt::red);
    QBuffer buffer(&result.image_data);
    if (buffer.open(QIODevice::WriteOnly)) {
      result.image.save(&buffer, "JPG");
      buffer.close();
    }
    result.mime_type = u"image/jpeg"_s;
    return result;
  }

  static void SetCoverLoaded(AlbumItem *album_item) { album_item->setData(AlbumCoverManager::Role_CoverLoaded, true); }

  static constexpr int kRoleArtEmbedded = AlbumCoverManager::Role_ArtEmbedded;
  static constexpr int kRoleArtAutomatic = AlbumCoverManager::Role_ArtAutomatic;
  static constexpr int kRoleArtManual = AlbumCoverManager::Role_ArtManual;
  static constexpr int kRoleArtUnset = AlbumCoverManager::Role_ArtUnset;
  static constexpr int kRoleAlbum = AlbumCoverManager::Role_Album;
  static constexpr int kRoleCuePath = AlbumCoverManager::Role_CuePath;

  QMainWindow mainwindow_;
  QThread *tagreader_client_thread_ = nullptr;
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

  AlbumItem *manual_album_item = AddAlbum();
  manual_album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  EXPECT_TRUE(ItemHasCover(manual_album_item));

  AlbumItem *automatic_album_item = AddAlbum();
  automatic_album_item->setData(kRoleArtAutomatic, QUrl::fromLocalFile(u"/music/cover.jpg"_s));
  EXPECT_TRUE(ItemHasCover(automatic_album_item));

  AlbumItem *embedded_album_item = AddAlbum();
  embedded_album_item->setData(kRoleArtEmbedded, true);
  EXPECT_TRUE(ItemHasCover(embedded_album_item));

}

TEST_F(AlbumCoverManagerTest, AlbumWithoutCoverHasNoCoverBeforeThumbnailIsLoaded) {

  EXPECT_FALSE(ItemHasCover(AddAlbum()));

  AlbumItem *unset_album_item = AddAlbum();
  unset_album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
  unset_album_item->setData(kRoleArtUnset, true);
  EXPECT_FALSE(ItemHasCover(unset_album_item));

}

// Only the cover types shown in the cover manager are used.
TEST_F(AlbumCoverManagerTest, CoverTypeNotShownIsNotUsedBeforeThumbnailIsLoaded) {

  SetCoverTypes(AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual);

  AlbumItem *embedded_album_item = AddAlbum();
  embedded_album_item->setData(kRoleArtEmbedded, true);
  EXPECT_FALSE(ItemHasCover(embedded_album_item));

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

  AlbumItem *unset_album_item = AddAlbum();
  AlbumItem *clear_album_item = AddAlbum();
  AlbumItem *deleted_album_item = AddAlbum();
  quint64 id = 0;
  for (AlbumItem *album_item : {unset_album_item, clear_album_item, deleted_album_item}) {
    album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
    StartThumbnailLoad(++id, album_item);
  }

  SetContextMenuItems(QList<QListWidgetItem*>() << unset_album_item);
  UnsetCover();
  SetContextMenuItems(QList<QListWidgetItem*>() << clear_album_item);
  ClearCover();
  SetContextMenuItems(QList<QListWidgetItem*>() << deleted_album_item);
  DeleteCover();
  EXPECT_EQ(0, RunningThumbnailLoads());

  for (quint64 loaded_id = 1; loaded_id <= id; ++loaded_id) {
    ThumbnailLoaded(loaded_id);
  }
  EXPECT_FALSE(ItemHasCover(unset_album_item));
  EXPECT_FALSE(ItemHasCover(clear_album_item));
  EXPECT_FALSE(ItemHasCover(deleted_album_item));

}

// Regression test: removing a cover while its thumbnail was loading didn't update the albums shown and the number of albums without covers, since the thumbnail load which updated them was canceled.
TEST_F(AlbumCoverManagerTest, RemovingCoverWhileThumbnailIsLoadingUpdatesFilter) {

  AlbumItem *unset_album_item = AddAlbum();
  AlbumItem *clear_album_item = AddAlbum();
  AlbumItem *deleted_album_item = AddAlbum();
  quint64 id = 0;
  for (AlbumItem *album_item : {unset_album_item, clear_album_item, deleted_album_item}) {
    album_item->setData(kRoleArtManual, QUrl::fromLocalFile(u"/covers/manual.jpg"_s));
    StartThumbnailLoad(++id, album_item);
  }
  ShowAlbumsWithCovers();
  UpdateFilter();
  EXPECT_EQ(u"0"_s, AlbumsWithoutCover());

  SetContextMenuItems(QList<QListWidgetItem*>() << unset_album_item);
  UnsetCover();
  EXPECT_TRUE(unset_album_item->isHidden());
  EXPECT_FALSE(clear_album_item->isHidden());

  SetContextMenuItems(QList<QListWidgetItem*>() << clear_album_item);
  ClearCover();
  EXPECT_TRUE(clear_album_item->isHidden());
  EXPECT_FALSE(deleted_album_item->isHidden());

  SetContextMenuItems(QList<QListWidgetItem*>() << deleted_album_item);
  DeleteCover();
  EXPECT_TRUE(deleted_album_item->isHidden());

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

// Regression test: embedding a cover in one copy of an album also marked the songs of the copy in another file type as having an embedded cover, the cover manager lists the copies as separate albums.
TEST_F(AlbumCoverManagerTest, EmbeddedCoverOnlyUpdatesSongsOfTheAlbumFiles) {

  collection_backend_->AddDirectory(u"/tmp"_s);
  SongList songs;
  int i = 0;
  for (const QString &extension : {u"flac"_s, u"mp3"_s}) {
    for (const QString &artist : {u"Artist 1"_s, u"Artist 2"_s}) {
      Song song(Song::Source::Collection);
      song.set_directory_id(1);
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_title(QStringLiteral("Title %1").arg(++i));
      song.set_artist(artist);
      song.set_album(u"Album"_s);
      song.set_compilation(true);
      song.set_url(QUrl::fromLocalFile(QStringLiteral("/tmp/%1.%2").arg(i).arg(extension)));
      songs << song;
    }
  }
  // A third FLAC song, saving the cover to this file fails.
  Song failed_song = songs[0];
  failed_song.set_title(u"Title 5"_s);
  failed_song.set_artist(u"Artist 3"_s);
  failed_song.set_url(QUrl::fromLocalFile(u"/tmp/5.flac"_s));
  songs << failed_song;
  collection_backend_->AddOrUpdateSongs(songs);

  // The FLAC copy of the compilation, as it's listed in the cover manager.
  AlbumItem *flac_album_item = AddAlbum();
  flac_album_item->setData(kRoleAlbum, u"Album"_s);
  flac_album_item->urls = QList<QUrl>() << songs[0].url() << songs[1].url() << songs[4].url();

  // All the saves are started before any of them finish, like when saving the cover, and the failed save finishes between the successful ones.
  QSignalSpy changed_spy(&*collection_backend_, &CollectionBackend::SongsChanged);
  for (const QUrl &url : std::as_const(flac_album_item->urls)) {
    StartEmbeddedCoverSave(flac_album_item, url);
  }
  FinishEmbeddedCoverSave(flac_album_item, songs[0].url(), true);
  FinishEmbeddedCoverSave(flac_album_item, songs[4].url(), false);
  FinishEmbeddedCoverSave(flac_album_item, songs[1].url(), true);
  ASSERT_TRUE(changed_spy.wait(5000));

  // One update for the files the cover was saved to.
  ASSERT_EQ(1, changed_spy.count());
  QStringList changed_titles;
  const SongList changed_songs = changed_spy.first().first().value<SongList>();
  for (const Song &song : changed_songs) {
    changed_titles << song.title();
  }
  changed_titles.sort();
  EXPECT_EQ(QStringList() << u"Title 1"_s << u"Title 2"_s, changed_titles);

  EXPECT_TRUE(collection_backend_->GetSongById(1).art_embedded());
  EXPECT_TRUE(collection_backend_->GetSongById(2).art_embedded());
  EXPECT_FALSE(collection_backend_->GetSongById(3).art_embedded());
  EXPECT_FALSE(collection_backend_->GetSongById(4).art_embedded());
  EXPECT_FALSE(collection_backend_->GetSongById(5).art_embedded());

}

// Regression test: covers were embedded in all the selected albums, also in file types which can't have embedded covers, so the cover was not saved for those albums.
TEST_F(AlbumCoverManagerTest, CoverIsSavedToFileForAlbumsWhichCantEmbedCovers) {

  TemporaryResource flac_file(u":/audio/strawberry.flac"_s);
  AlbumItem *flac_album_item = AddAlbum();
  SetAlbum(flac_album_item, u"FLAC Album"_s, Song::FileType::FLAC, QUrl::fromLocalFile(flac_file.fileName()));
  AlbumItem *ape_album_item = AddAlbum();
  SetAlbum(ape_album_item, u"APE Album"_s, Song::FileType::APE, QUrl::fromLocalFile(u"/music/song.ape"_s));
  AlbumItem *cue_album_item = AddAlbum();
  SetAlbum(cue_album_item, u"CUE Album"_s, Song::FileType::FLAC, QUrl::fromLocalFile(u"/music/cue.flac"_s));
  cue_album_item->setData(kRoleCuePath, u"/music/cue.cue"_s);
  // The file of the CUE sheet already has an embedded cover, which can't be replaced.
  cue_album_item->setData(kRoleArtEmbedded, true);
  collection_backend_->AddDirectory(u"/music"_s);
  Song cue_song(Song::Source::Collection);
  cue_song.set_directory_id(1);
  cue_song.set_mtime(1);
  cue_song.set_ctime(1);
  cue_song.set_filesize(1);
  cue_song.set_title(u"Title"_s);
  cue_song.set_artist(u"Artist"_s);
  cue_song.set_album(u"CUE Album"_s);
  cue_song.set_url(QUrl::fromLocalFile(u"/music/cue.flac"_s));
  cue_song.set_cue_path(u"/music/cue.cue"_s);
  cue_song.set_art_embedded(true);
  collection_backend_->AddOrUpdateSongs(SongList() << cue_song);
  ASSERT_TRUE(collection_backend_->GetSongById(1).art_embedded());

  // The cover files are saved in the image cache, which is only redirected for the tests on Linux and BSD, keep them out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);

  SetSaveEmbeddedCover();
  SetContextMenuItems(QList<QListWidgetItem*>() << flac_album_item << ape_album_item << cue_album_item);
  const AlbumCoverImageResult result = JpegCover();
  ASSERT_FALSE(result.image_data.isEmpty());
  QSignalSpy changed_spy(&*collection_backend_, &CollectionBackend::SongsChanged);
  SaveImageToAlbums(result);

  // The cover is embedded in the FLAC album.
  EXPECT_TRUE(CoverIsBeingEmbedded(flac_album_item));
  ASSERT_TRUE(QTest::qWaitFor([this, flac_album_item]() { return !CoverIsBeingEmbedded(flac_album_item); }, 10000));
  EXPECT_TRUE(flac_album_item->data(kRoleArtEmbedded).toBool());
  EXPECT_TRUE(flac_album_item->data(kRoleArtManual).toUrl().isEmpty());
  QByteArray embedded_cover_data;
  ASSERT_TRUE(tagreader_client_->LoadCoverDataBlocking(flac_file.fileName(), embedded_cover_data).success());
  EXPECT_EQ(result.image_data, embedded_cover_data);

  // The embedded cover of the CUE sheet album is no longer used, so the new cover file is shown instead of it, the file is not changed.
  const QUrl cue_cover_url = cue_album_item->data(kRoleArtManual).toUrl();
  EXPECT_FALSE(cue_album_item->data(kRoleArtEmbedded).toBool());
  // The songs read from the collection get a cover file in the image cache as manual cover, also before the collection is updated, so wait for the updates, the embedded cover and the manual cover of the CUE sheet album.
  ASSERT_TRUE(QTest::qWaitFor([&changed_spy]() { return changed_spy.count() >= 2; }, 10000));
  EXPECT_EQ(cue_cover_url, collection_backend_->GetSongById(1).art_manual());
  EXPECT_FALSE(collection_backend_->GetSongById(1).art_embedded());

  // The APE album and the CUE sheet album get a cover file instead.
  for (AlbumItem *album_item : {ape_album_item, cue_album_item}) {
    EXPECT_FALSE(CoverIsBeingEmbedded(album_item));
    const QUrl cover_url = album_item->data(kRoleArtManual).toUrl();
    ASSERT_TRUE(cover_url.isLocalFile()) << cover_url.toString().toStdString();
    EXPECT_TRUE(QFileInfo::exists(cover_url.toLocalFile())) << cover_url.toLocalFile().toStdString();
    EXPECT_TRUE(QFile::remove(cover_url.toLocalFile()));
  }

}

// Regression test: when the cover was saved to a file for an album which can't have embedded covers, the embedded cover was no longer used for all the songs with the same album artist and album, also for a copy of the album in another file type, which is listed as another album.
TEST_F(AlbumCoverManagerTest, CoverFileForAlbumOnlyChangesTheFilesOfTheAlbum) {

  // A CUE sheet album and an MP3 copy of it, with the same tags, both files have embedded covers.
  TemporaryResource mp3_file(u":/audio/strawberry.mp3"_s);
  collection_backend_->AddDirectory(u"/music"_s);
  Song cue_song(Song::Source::Collection);
  cue_song.set_directory_id(1);
  cue_song.set_mtime(1);
  cue_song.set_ctime(1);
  cue_song.set_filesize(1);
  cue_song.set_title(u"Title"_s);
  cue_song.set_artist(u"Artist"_s);
  cue_song.set_album(u"Album"_s);
  cue_song.set_url(QUrl::fromLocalFile(u"/music/cue.flac"_s));
  cue_song.set_cue_path(u"/music/cue.cue"_s);
  cue_song.set_art_embedded(true);
  Song mp3_song = cue_song;
  mp3_song.set_url(QUrl::fromLocalFile(mp3_file.fileName()));
  mp3_song.set_cue_path(QString());
  collection_backend_->AddOrUpdateSongs(SongList() << cue_song << mp3_song);
  ASSERT_TRUE(collection_backend_->GetSongById(1).art_embedded());
  ASSERT_TRUE(collection_backend_->GetSongById(2).art_embedded());

  AlbumItem *cue_album_item = AddAlbum();
  SetAlbum(cue_album_item, u"Album"_s, Song::FileType::FLAC, cue_song.url());
  cue_album_item->setData(kRoleCuePath, cue_song.cue_path());
  cue_album_item->setData(kRoleArtEmbedded, true);
  AlbumItem *mp3_album_item = AddAlbum();
  SetAlbum(mp3_album_item, u"Album"_s, Song::FileType::MPEG, mp3_song.url());
  mp3_album_item->setData(kRoleArtEmbedded, true);

  // The cover files are saved in the image cache, which is only redirected for the tests on Linux and BSD, keep them out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);
  SetSaveEmbeddedCover();
  const AlbumCoverImageResult result = JpegCover();
  ASSERT_FALSE(result.image_data.isEmpty());

  // Only the CUE sheet album is selected, the embedded cover of the MP3 copy is still used.
  SetContextMenuItems(QList<QListWidgetItem*>() << cue_album_item);
  QSignalSpy changed_spy(&*collection_backend_, &CollectionBackend::SongsChanged);
  SaveImageToAlbums(result);
  const QUrl cover_url = cue_album_item->data(kRoleArtManual).toUrl();
  ASSERT_TRUE(cover_url.isLocalFile()) << cover_url.toString().toStdString();
  // The songs read from the collection get a cover file in the image cache as manual cover, also before the collection is updated, so wait for the updates, the embedded cover and the manual cover.
  ASSERT_TRUE(QTest::qWaitFor([&changed_spy]() { return changed_spy.count() >= 2; }, 10000));
  EXPECT_EQ(cover_url, collection_backend_->GetSongById(1).art_manual());
  EXPECT_FALSE(collection_backend_->GetSongById(1).art_embedded());
  EXPECT_TRUE(collection_backend_->GetSongById(2).art_embedded());
  EXPECT_TRUE(mp3_album_item->data(kRoleArtEmbedded).toBool());

  // Both albums are selected, the cover is embedded in the MP3 copy, and the CUE sheet album keeps using the cover file.
  SetContextMenuItems(QList<QListWidgetItem*>() << mp3_album_item << cue_album_item);
  SaveImageToAlbums(result);
  ASSERT_TRUE(QTest::qWaitFor([this, mp3_album_item]() { return !CoverIsBeingEmbedded(mp3_album_item); }, 10000));
  QByteArray embedded_cover_data;
  ASSERT_TRUE(tagreader_client_->LoadCoverDataBlocking(mp3_file.fileName(), embedded_cover_data).success());
  EXPECT_EQ(result.image_data, embedded_cover_data);
  ASSERT_TRUE(QTest::qWaitFor([this]() { return collection_backend_->GetSongById(2).art_embedded(); }, 10000));
  // The collection is updated asynchronously, give it time to apply a wrong update.
  QTest::qWait(500);
  EXPECT_FALSE(collection_backend_->GetSongById(1).art_embedded());
  EXPECT_FALSE(cue_album_item->data(kRoleArtEmbedded).toBool());

  EXPECT_TRUE(QFile::remove(cover_url.toLocalFile()));

}

// Regression test: the embedded cover was only no longer used when the first file of the album had an embedded cover, the album only has the state of its first file, so the embedded cover of another file of the album was still shown instead of the new cover file.
TEST_F(AlbumCoverManagerTest, CoverFileForAlbumIsUsedForAllFilesOfTheAlbum) {

  // An APE album where only the second file has an embedded cover.
  collection_backend_->AddDirectory(u"/music"_s);
  SongList songs;
  for (int i = 1; i <= 2; ++i) {
    Song song(Song::Source::Collection);
    song.set_directory_id(1);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_filesize(1);
    song.set_title(QStringLiteral("Title %1").arg(i));
    song.set_artist(u"Artist"_s);
    song.set_album(u"Album"_s);
    song.set_url(QUrl::fromLocalFile(QStringLiteral("/music/%1.ape").arg(i)));
    song.set_art_embedded(i == 2);
    songs << song;
  }
  collection_backend_->AddOrUpdateSongs(songs);
  ASSERT_FALSE(collection_backend_->GetSongById(1).art_embedded());
  ASSERT_TRUE(collection_backend_->GetSongById(2).art_embedded());

  // The album item has the state of the first file, like the albums listed by CollectionBackend::GetAlbums().
  AlbumItem *ape_album_item = AddAlbum();
  SetAlbum(ape_album_item, u"Album"_s, Song::FileType::APE, songs[0].url());
  ape_album_item->urls << songs[1].url();
  ape_album_item->setData(kRoleArtEmbedded, false);

  // The cover files are saved in the image cache, which is only redirected for the tests on Linux and BSD, keep them out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);
  SetSaveEmbeddedCover();
  const AlbumCoverImageResult result = JpegCover();
  ASSERT_FALSE(result.image_data.isEmpty());

  SetContextMenuItems(QList<QListWidgetItem*>() << ape_album_item);
  QSignalSpy changed_spy(&*collection_backend_, &CollectionBackend::SongsChanged);
  SaveImageToAlbums(result);
  const QUrl cover_url = ape_album_item->data(kRoleArtManual).toUrl();
  ASSERT_TRUE(cover_url.isLocalFile()) << cover_url.toString().toStdString();
  // The songs read from the collection get a cover file in the image cache as manual cover, also before the collection is updated, so wait for the updates, the embedded cover and the manual cover.
  ASSERT_TRUE(QTest::qWaitFor([&changed_spy]() { return changed_spy.count() >= 2; }, 10000));

  // The cover file is used for both files, the embedded cover of the second file is no longer used.
  for (const int id : {1, 2}) {
    EXPECT_EQ(cover_url, collection_backend_->GetSongById(id).art_manual());
    EXPECT_FALSE(collection_backend_->GetSongById(id).art_embedded());
  }

  EXPECT_TRUE(QFile::remove(cover_url.toLocalFile()));

}

}  // namespace
