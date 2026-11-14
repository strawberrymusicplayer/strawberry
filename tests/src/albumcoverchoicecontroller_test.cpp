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

#include "config.h"

#include <memory>
#include <functional>
#include <optional>

#include <utility>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QThread>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QStandardPaths>
#include <QStringList>
#include <QByteArray>
#include <QUrl>
#include <QDialogButtonBox>
#include <QFile>
#include <QString>
#include <QImage>
#include <QColor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QApplication>
#include <QMessageBox>
#include <QAbstractButton>
#include <QBuffer>
#include <QImageReader>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreaderloadcoverimagereply.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverimageresult.h"
#include "covermanager/albumcoverimageresult.h"
#include "covermanager/confirmedcoverfiles.h"
#include "streaming/streamingservices.h"
#include "utilities/imageutils.h"

#include "deleteconfirmationanswer.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

class AlbumCoverChoiceControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tagreader_client_ = SharedPtr<TagReaderClient>(new TagReaderClient(), [](TagReaderClient *tagreader_client) { tagreader_client->deleteLater(); });
    tagreader_client_thread_ = new QThread();
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<MemoryDatabase>(nullptr);
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    albumcover_loader_ = make_shared<AlbumCoverLoader>(tagreader_client_);
  }

  void TearDown() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  // Processes events until the condition is true, returns false on timeout.
  template <typename Condition>
  static bool WaitFor(Condition condition, const int timeout_msec = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > timeout_msec) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
  }

  // Returns true when the embedded cover of the file is mostly the color.
  // The cover is loaded in the tagreader thread, after the cover being saved there, since loading it here at the same time can make the save fail on Windows, where TagLib can't open a file for writing while it's open elsewhere.
  bool HasEmbeddedCoverColor(const QString &filename, const QColor &color) const {
    // The Finished signal is queued to this thread, so it's not emitted before it's connected here.
    TagReaderLoadCoverImageReplyPtr reply = tagreader_client_->LoadCoverImageAsync(filename);
    // The wait is bounded, so the test fails instead of hanging when Finished is not emitted.
    constexpr int kTimeoutMsec = 30000;
    bool finished = false;
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderLoadCoverImageReply::Finished, &loop, [&loop, &finished]() {
      finished = true;
      loop.quit();
    });
    QTimer::singleShot(kTimeoutMsec, &loop, &QEventLoop::quit);
    loop.exec();
    if (!finished) {
      ADD_FAILURE() << "Timed out loading the embedded cover of " << filename.toStdString();
      return false;
    }
    const QImage image = reply->image();
    if (!reply->success() || image.isNull()) return false;
    const QColor pixel = image.pixelColor(image.width() / 2, image.height() / 2);
    return qAbs(pixel.red() - color.red()) < 32 && qAbs(pixel.green() - color.green()) < 32 && qAbs(pixel.blue() - color.blue()) < 32;
  }

  static QImage CreateImage(const QColor &color) {
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(color);
    return image;
  }

  QThread *tagreader_client_thread_ = nullptr;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
};

// Regression test: a cover result with only an image, like an image dropped on the cover, was saved as empty cover data, which removed the embedded cover instead of replacing it.
TEST_F(AlbumCoverChoiceControllerTest, EmbeddingImageOnlyResultReplacesCover) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  // The file has an embedded cover.
  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  controller.SaveCoverEmbeddedToCollectionSongs(song, QString(), blue_image_data, u"image/jpeg"_s);
  ASSERT_TRUE(WaitFor([this, &filename]() { return HasEmbeddedCoverColor(filename, Qt::blue); }));

  // An image without the image data replaces the embedded cover.
  controller.SaveCoverEmbeddedToCollectionSongs(song, AlbumCoverImageResult(CreateImage(Qt::red)));
  EXPECT_TRUE(WaitFor([this, &filename]() { return HasEmbeddedCoverColor(filename, Qt::red); }));

}

// Regression test: the songs of the album were looked up without the compilations, so saving the embedded cover of a compilation song from the player did nothing.
TEST_F(AlbumCoverChoiceControllerTest, EmbeddingCoverSavesToCompilationSongs) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  // The songs of the album are read in another thread, which has its own database with the in-memory database, so the database is in a file.
  // The in-memory database is deleted first, it reports all open connections when it's deleted.
  collection_backend_.reset();
  database_.reset();
  database_ = make_shared<Database>(task_manager_, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  // Unlike the in-memory database, the connection isn't closed when the database is deleted, it's closed before the directory with the database file is removed, also when the test returns early.
  const QScopeGuard close_database([this]() { database_->Close(); });
  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  collection_backend_->AddDirectory(temp_dir.path());

  SongList songs;
  for (int i = 1; i <= 2; ++i) {
    const QString filename = temp_dir.filePath(u"song%1.mp3"_s.arg(i));
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    Song song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
    ASSERT_TRUE(song.save_embedded_cover_supported());
    song.set_source(Song::Source::Collection);
    song.set_directory_id(1);
    song.set_title(u"Title %1"_s.arg(i));
    song.set_artist(u"Artist %1"_s.arg(i));
    song.set_albumartist(u"Various Artists"_s);
    song.set_album(u"Compilation"_s);
    song.set_compilation(true);
    song.set_art_embedded(false);
    songs << song;
  }
  collection_backend_->AddOrUpdateSongs(songs);
  ASSERT_EQ(2, collection_backend_->GetAllSongs().count());
  ASSERT_TRUE(collection_backend_->GetAllSongs().first().is_compilation());

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);

  controller.SaveCoverEmbeddedToCollectionSongs(songs.first(), AlbumCoverImageResult(QUrl(), QString(), QByteArray(), CreateImage(Qt::blue)));
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 2; }));
  EXPECT_TRUE(spy_embedded_cover_saved.at(0).at(1).toBool());
  EXPECT_TRUE(spy_embedded_cover_saved.at(1).at(1).toBool());
  for (const Song &song : std::as_const(songs)) {
    EXPECT_TRUE(HasEmbeddedCoverColor(song.url().toLocalFile(), Qt::blue)) << song.url().toLocalFile().toStdString();
  }

  // Removing the embedded cover removes it from the compilation songs too.
  controller.SaveCoverEmbeddedToCollectionSongs(songs.first(), AlbumCoverImageResult());
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 4; }));
  EXPECT_TRUE(spy_embedded_cover_saved.at(2).at(1).toBool());
  EXPECT_TRUE(spy_embedded_cover_saved.at(3).at(1).toBool());
  // The files are read to check that they have no embedded cover, a failed read or a different cover doesn't pass.
  for (const Song &song : std::as_const(songs)) {
    Song read_song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client_->ReadFileBlocking(song.url().toLocalFile(), &read_song).success()) << song.url().toLocalFile().toStdString();
    EXPECT_FALSE(read_song.art_embedded()) << song.url().toLocalFile().toStdString();
  }

}

// Regression test: embedding a cover from a file, like a cover loaded from disk, stored the song as not having an embedded cover, since only the image data was checked.
TEST_F(AlbumCoverChoiceControllerTest, EmbeddingCoverFromFileSetsArtEmbedded) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
  const QString cover_filename = temp_dir.filePath(u"cover.jpg"_s);
  ASSERT_TRUE(CreateImage(Qt::green).save(cover_filename, "JPG"));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  song.set_artist(u"Artist"_s);
  song.set_album(u"Album"_s);
  song.set_art_embedded(false);
  collection_backend_->AddOrUpdateSongs(SongList() << song);
  ASSERT_EQ(collection_backend_->GetAllSongs().count(), 1);
  ASSERT_FALSE(collection_backend_->GetAllSongs().first().art_embedded());

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  controller.SaveCoverEmbeddedToSong(song, cover_filename, QByteArray());
  ASSERT_TRUE(WaitFor([this, &filename]() { return HasEmbeddedCoverColor(filename, Qt::green); }));
  EXPECT_TRUE(WaitFor([this]() { return collection_backend_->GetAllSongs().first().art_embedded(); }));

}

// Regression test: when the cover was embedded in a song without an artist, the songs of the album were updated by album artist and album, which also updated the compilations with the same album name, although their files were not changed.
TEST_F(AlbumCoverChoiceControllerTest, EmbeddingCoverOnlyUpdatesSongsInTheFile) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  song.set_artist(QString());
  song.set_albumartist(QString());
  song.set_album(u"Album"_s);
  song.set_compilation(false);
  song.set_art_embedded(false);

  // A compilation with the same album name, its file is not changed.
  Song compilation_song = song;
  compilation_song.set_title(u"Compilation title"_s);
  compilation_song.set_artist(u"Other artist"_s);
  compilation_song.set_compilation(true);
  compilation_song.set_art_unset(true);
  compilation_song.set_url(QUrl::fromLocalFile(temp_dir.filePath(u"compilation.mp3"_s)));
  collection_backend_->AddOrUpdateSongs(SongList() << song << compilation_song);
  ASSERT_EQ(2, collection_backend_->GetAllSongs().count());
  ASSERT_EQ(1, collection_backend_->GetSongsByUrl(compilation_song.url()).count());

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);

  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  controller.SaveCoverEmbeddedToSong(song, QString(), blue_image_data, u"image/jpeg"_s);
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 1; }));
  EXPECT_TRUE(spy_embedded_cover_saved.first().at(1).toBool());
  EXPECT_TRUE(HasEmbeddedCoverColor(filename, Qt::blue));
  ASSERT_TRUE(WaitFor([this, &song]() { return collection_backend_->GetSongsByUrl(song.url()).value(0).art_embedded(); }));

  // The collection is updated asynchronously, give it time to apply a wrong update.
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  const Song stored_compilation_song = collection_backend_->GetSongsByUrl(compilation_song.url()).value(0);
  EXPECT_FALSE(stored_compilation_song.art_embedded());
  EXPECT_TRUE(stored_compilation_song.art_unset());

}

// Regression test: when saves to the same song overlapped, like when removing a cover right after setting it, the first save that finished removed both saves, so the result of the last save was ignored and the song was stored with the embedded cover of the first save.
TEST_F(AlbumCoverChoiceControllerTest, OverlappingSavesKeepResultOfLastSave) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  song.set_artist(u"Artist"_s);
  song.set_album(u"Album"_s);
  song.set_art_embedded(false);
  collection_backend_->AddOrUpdateSongs(SongList() << song);
  ASSERT_EQ(collection_backend_->GetAllSongs().count(), 1);

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);

  // Set a cover and remove it again before the first save finished.
  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  controller.SaveCoverEmbeddedToSong(song, QString(), blue_image_data, u"image/jpeg"_s);
  controller.SaveCoverEmbeddedToSong(song, QString(), QByteArray());

  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 2; }));
  EXPECT_TRUE(spy_embedded_cover_saved.at(0).at(1).toBool());
  EXPECT_TRUE(spy_embedded_cover_saved.at(1).at(1).toBool());
  EXPECT_FALSE(HasEmbeddedCoverColor(filename, Qt::blue));

  // The collection is updated asynchronously, give it time to apply a wrong update.
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  EXPECT_FALSE(collection_backend_->GetAllSongs().first().art_embedded());

}

// Regression test: when a save to a song succeeded while a newer save to the same song was pending, and the newer save failed, the result of the successful save was not stored, so the song was stored without the embedded cover it has.
TEST_F(AlbumCoverChoiceControllerTest, OverlappingSavesKeepLastSuccessfulResult) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  song.set_artist(u"Artist"_s);
  song.set_album(u"Album"_s);
  song.set_art_embedded(false);
  collection_backend_->AddOrUpdateSongs(SongList() << song);
  ASSERT_EQ(collection_backend_->GetAllSongs().count(), 1);

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);

  // Set a cover, then set a cover from a file which does not exist before the first save finished, so the second save fails.
  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  controller.SaveCoverEmbeddedToSong(song, QString(), blue_image_data, u"image/jpeg"_s);
  controller.SaveCoverEmbeddedToSong(song, temp_dir.filePath(u"missing.jpg"_s), QByteArray());

  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 2; }));
  EXPECT_TRUE(spy_embedded_cover_saved.at(0).at(1).toBool());
  EXPECT_FALSE(spy_embedded_cover_saved.at(1).at(1).toBool());
  EXPECT_TRUE(HasEmbeddedCoverColor(filename, Qt::blue));
  EXPECT_TRUE(WaitFor([this]() { return collection_backend_->GetAllSongs().first().art_embedded(); }));

}

// Regression test: deleting and unsetting an embedded cover reloaded the current cover with the song copied before the cover was unset when the embedded cover was removed, so the cover was no longer unset and could be fetched again.
TEST_F(AlbumCoverChoiceControllerTest, DeletingEmbeddedCoverKeepsCurrentCoverUnset) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  song.set_artist(u"Artist"_s);
  song.set_album(u"Album"_s);

  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader = make_shared<CurrentAlbumCoverLoader>(albumcover_loader_);
  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, current_albumcover_loader, make_shared<CoverProviders>(), make_shared<StreamingServices>());

  // The song is the current song, and the file has an embedded cover.
  current_albumcover_loader->LoadAlbumCover(song);
  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  controller.SaveCoverEmbeddedToCollectionSongs(song, QString(), blue_image_data, u"image/jpeg"_s);
  ASSERT_TRUE(WaitFor([&current_albumcover_loader]() { return current_albumcover_loader->last_song().art_embedded(); }));
  ASSERT_TRUE(HasEmbeddedCoverColor(filename, Qt::blue));
  song.set_art_embedded(true);

  // Delete and unset the cover, the embedded cover is removed after the cover is unset.
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);
  ASSERT_TRUE(controller.DeleteCover(&song, true));
  EXPECT_TRUE(song.art_unset());
  EXPECT_TRUE(current_albumcover_loader->last_song().art_unset());
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 1; }));
  EXPECT_TRUE(spy_embedded_cover_saved.first().at(1).toBool());
  EXPECT_FALSE(HasEmbeddedCoverColor(filename, Qt::blue));

  EXPECT_TRUE(current_albumcover_loader->last_song().art_unset());
  EXPECT_FALSE(current_albumcover_loader->last_song().art_embedded());

}

// Regression test: deleting and unsetting the cover of an album, like from the album cover manager, did not reload the current cover when the current song was another song from the album.
TEST_F(AlbumCoverChoiceControllerTest, DeletingAlbumCoverUnsetsCurrentAlbumSong) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  // The album songs are read from the collection in another thread, so use a database file, a memory database is empty in other threads.
  collection_backend_.reset();
  database_.reset();
  database_ = make_shared<Database>(task_manager_, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  collection_backend_->AddDirectory(temp_dir.path());

  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader = make_shared<CurrentAlbumCoverLoader>(albumcover_loader_);
  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, current_albumcover_loader, make_shared<CoverProviders>(), make_shared<StreamingServices>());
  QSignalSpy spy_embedded_cover_saved(&controller, &AlbumCoverChoiceController::EmbeddedCoverSaved);

  // Two songs from the same album in the collection, with an embedded cover.
  const QByteArray blue_image_data = ImageUtils::SaveImageToJpegData(CreateImage(Qt::blue));
  ASSERT_FALSE(blue_image_data.isEmpty());
  SongList songs;
  for (const QString &name : {u"first.mp3"_s, u"second.mp3"_s}) {
    const QString filename = temp_dir.filePath(name);
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    Song song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
    ASSERT_TRUE(song.save_embedded_cover_supported());
    controller.SaveCoverEmbeddedToSong(song, QString(), blue_image_data, u"image/jpeg"_s);
    song.set_source(Song::Source::Collection);
    song.set_directory_id(1);
    song.set_artist(u"Artist"_s);
    song.set_album(u"Album"_s);
    song.set_art_embedded(true);
    songs << song;
  }
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 2; }));
  spy_embedded_cover_saved.clear();
  collection_backend_->AddOrUpdateSongs(songs);
  const SongList collection_songs = collection_backend_->GetAllSongs();
  ASSERT_EQ(2, collection_songs.count());

  // The second song is the current song, the cover is deleted for the album from the first song.
  Song current_song = collection_songs.value(1);
  ASSERT_TRUE(current_song.art_embedded());
  current_albumcover_loader->LoadAlbumCover(current_song);
  Song album_song = collection_songs.value(0);
  ASSERT_TRUE(controller.DeleteCover(&album_song, true));

  EXPECT_TRUE(current_albumcover_loader->last_song() == current_song);
  EXPECT_TRUE(current_albumcover_loader->last_song().art_unset());
  EXPECT_FALSE(current_albumcover_loader->last_song().art_embedded());

  // The embedded covers of both songs are removed after the cover is unset, and the current cover stays unset.
  ASSERT_TRUE(WaitFor([&spy_embedded_cover_saved]() { return spy_embedded_cover_saved.count() == 2; }));
  EXPECT_TRUE(current_albumcover_loader->last_song().art_unset());
  EXPECT_FALSE(current_albumcover_loader->last_song().art_embedded());

  database_->Close();

}

// Creates the directory of the file and saves an image to the file, returns false when it couldn't be saved.
bool SaveImageFile(const QString &filename) {

  if (!QDir().mkpath(QFileInfo(filename).path())) return false;
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  return image.save(filename, "JPG");

}

// Regression test: deleting the cover deleted the user's own image used as cover, like an image in the album directory or another directory, without asking.
// Only the cover files outside the album cover cache directory are listed for asking, the files in the cache are Strawberry's own copies.
TEST_F(AlbumCoverChoiceControllerTest, CoverFilesOutsideCacheSkipsCacheFiles) {

  // The cache is only redirected for the tests on Linux and BSD, keep the files out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString cache_dir = Song::ImageCacheDir(Song::Source::Collection);
  const QString album_cover = temp_dir.filePath(u"album/cover.jpg"_s);
  const QString cache_cover = cache_dir + u"/cache-cover-test.jpg"_s;
  // A directory starting with the name of the cache directory, and a path through the cache directory to a file outside of it.
  const QString similar_dir_cover = cache_dir + u"-other/cover-test.jpg"_s;
  const QString parent_dir_cover = QFileInfo(cache_dir).path() + u"/parent-cover-test.jpg"_s;
  const QString parent_dir_cover_through_cache = cache_dir + u"/../parent-cover-test.jpg"_s;
  for (const QString &filename : {album_cover, cache_cover, similar_dir_cover, parent_dir_cover}) {
    ASSERT_TRUE(SaveImageFile(filename)) << filename.toStdString();
  }

  Song song1(Song::Source::Collection);
  song1.set_art_automatic(QUrl::fromLocalFile(album_cover));
  song1.set_art_manual(QUrl::fromLocalFile(cache_cover));
  // The same cover as the first song is only listed once.
  Song song2(Song::Source::Collection);
  song2.set_art_manual(QUrl::fromLocalFile(album_cover));
  // A cover file which doesn't exist is not deleted.
  Song song3(Song::Source::Collection);
  song3.set_art_manual(QUrl::fromLocalFile(temp_dir.filePath(u"missing.jpg"_s)));
  Song song4(Song::Source::Collection);
  song4.set_art_automatic(QUrl::fromLocalFile(similar_dir_cover));
  song4.set_art_manual(QUrl::fromLocalFile(parent_dir_cover_through_cache));

  // A song from a source without a cache directory.
  Song song5(Song::Source::Device);
  ASSERT_FALSE(QFileInfo::exists(Song::ImageCacheDir(Song::Source::Device)));
  const QString device_cover = temp_dir.filePath(u"device/cover.jpg"_s);
  ASSERT_TRUE(SaveImageFile(device_cover));
  song5.set_art_manual(QUrl::fromLocalFile(device_cover));

  EXPECT_EQ(QStringList() << album_cover << similar_dir_cover << parent_dir_cover_through_cache << device_cover, AlbumCoverChoiceController::CoverFilesOutsideCache(SongList() << song1 << song2 << song3 << song4 << song5));

#ifndef Q_OS_WIN
  // A cache file through a symbolic link to the cache directory is in the cache.
  const QString cache_link = temp_dir.filePath(u"cache-link"_s);
  ASSERT_TRUE(QFile::link(cache_dir, cache_link));
  Song song6(Song::Source::Collection);
  song6.set_art_manual(QUrl::fromLocalFile(cache_link + u"/cache-cover-test.jpg"_s));
  EXPECT_TRUE(AlbumCoverChoiceController::CoverFilesOutsideCache(SongList() << song6).isEmpty());

  // Deleting a symbolic link deletes the link, so a link outside the cache to a cache file is outside the cache, and a link in the cache to a file outside of it is in the cache.
  const QString link_to_cache_cover = temp_dir.filePath(u"album/link-to-cache.jpg"_s);
  ASSERT_TRUE(QFile::link(cache_cover, link_to_cache_cover));
  const QString cache_link_to_album_cover = cache_dir + u"/link-to-album-cover-test.jpg"_s;
  ASSERT_TRUE(QFile::link(album_cover, cache_link_to_album_cover));
  Song song7(Song::Source::Collection);
  song7.set_art_automatic(QUrl::fromLocalFile(link_to_cache_cover));
  song7.set_art_manual(QUrl::fromLocalFile(cache_link_to_album_cover));
  EXPECT_EQ(QStringList() << link_to_cache_cover, AlbumCoverChoiceController::CoverFilesOutsideCache(SongList() << song7));
  QFile::remove(cache_link_to_album_cover);
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  // A cache file is in the cache when the path of the cache directory has a symbolic link, like a data directory which is a symbolic link.
  const QByteArray data_home = qgetenv("XDG_DATA_HOME");
  const QString data_home_link = temp_dir.filePath(u"data-home-link"_s);
  ASSERT_TRUE(QFile::link(QString::fromLocal8Bit(data_home), data_home_link));
  qputenv("XDG_DATA_HOME", data_home_link.toLocal8Bit());
  const bool cache_cover_outside_cache = AlbumCoverChoiceController::CoverFilesOutsideCache(SongList() << song1).contains(cache_cover);
  qputenv("XDG_DATA_HOME", data_home);
  EXPECT_FALSE(cache_cover_outside_cache);
#endif
  EXPECT_TRUE(AlbumCoverChoiceController::CoverFilesOutsideCache(SongList() << song3).isEmpty());

  for (const QString &filename : {cache_cover, similar_dir_cover, parent_dir_cover}) {
    QFile::remove(filename);
  }
  QDir().rmdir(QFileInfo(similar_dir_cover).path());

  QStandardPaths::setTestModeEnabled(false);

}

// Deleting a cover file outside the album cover cache directory asks first, a cover file in the cache is deleted without asking.
TEST_F(AlbumCoverChoiceControllerTest, ConfirmDeleteCoverAsksForFilesOutsideCache) {

  QStandardPaths::setTestModeEnabled(true);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString album_cover = temp_dir.filePath(u"cover.jpg"_s);
  const QString cache_cover = Song::ImageCacheDir(Song::Source::Collection) + u"/cache-cover-test.jpg"_s;
  ASSERT_TRUE(SaveImageFile(album_cover));
  ASSERT_TRUE(SaveImageFile(cache_cover));

  Song album_song(Song::Source::Collection);
  album_song.set_url(QUrl::fromLocalFile(temp_dir.filePath(u"song.flac"_s)));
  album_song.set_albumartist(u"Artist"_s);
  album_song.set_album(u"Album"_s);
  album_song.set_art_automatic(QUrl::fromLocalFile(album_cover));
  Song cache_song = album_song;
  cache_song.clear_art_automatic();
  cache_song.set_art_manual(QUrl::fromLocalFile(cache_cover));

  AlbumCoverChoiceController controller;

  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Cancel);
    EXPECT_FALSE(controller.ConfirmDeleteCover(SongList() << album_song << cache_song).has_value());
    EXPECT_EQ(1, answer.shown());
    EXPECT_EQ(QStringList() << album_cover, answer.files());
  }
  EXPECT_TRUE(QFile::exists(album_cover));

  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    const std::optional<ConfirmedCoverFiles> confirmed_files = controller.ConfirmDeleteCover(SongList() << album_song);
    ASSERT_TRUE(confirmed_files.has_value());
    EXPECT_TRUE(confirmed_files->contains(album_cover));
    EXPECT_EQ(1, answer.shown());
  }

  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Cancel);
    const std::optional<ConfirmedCoverFiles> confirmed_files = controller.ConfirmDeleteCover(SongList() << cache_song);
    ASSERT_TRUE(confirmed_files.has_value());
    EXPECT_FALSE(confirmed_files->contains(cache_cover));
    EXPECT_EQ(0, answer.shown());
  }

  QFile::remove(cache_cover);

  QStandardPaths::setTestModeEnabled(false);

}

// Regression test: the cover could change while the user was asked, and then the new cover file was deleted, which the user wasn't asked about.
// The cover is only deleted when the user confirmed deleting the cover files outside the cache, the cover files in the cache are deleted without asking.
TEST_F(AlbumCoverChoiceControllerTest, DeleteCoverOnlyDeletesConfirmedFilesOutsideCache) {

  QStandardPaths::setTestModeEnabled(true);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString album_cover = temp_dir.filePath(u"cover.jpg"_s);
  const QString cache_cover = Song::ImageCacheDir(Song::Source::Collection) + u"/cache-cover-test.jpg"_s;

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  for (const bool confirmed : {false, true}) {
    ASSERT_TRUE(SaveImageFile(album_cover));
    ASSERT_TRUE(SaveImageFile(cache_cover));
    Song song(Song::Source::Collection);
    song.set_url(QUrl::fromLocalFile(temp_dir.filePath(u"song.flac"_s)));
    song.set_albumartist(u"Artist"_s);
    song.set_album(u"Album"_s);
    song.set_art_automatic(QUrl::fromLocalFile(album_cover));
    song.set_art_manual(QUrl::fromLocalFile(cache_cover));
    // Without confirming, nothing of the cover is deleted, also not the cover file in the cache.
    EXPECT_EQ(confirmed, controller.DeleteCover(&song, false, confirmed ? ConfirmedCoverFiles(QStringList() << album_cover) : ConfirmedCoverFiles()));
    EXPECT_EQ(!confirmed, QFile::exists(album_cover));
    EXPECT_EQ(!confirmed, QFile::exists(cache_cover));
    EXPECT_EQ(!confirmed, song.art_automatic().isValid());
    EXPECT_EQ(!confirmed, song.art_manual().isValid());
  }
  QFile::remove(cache_cover);

  database_->Close();

  QStandardPaths::setTestModeEnabled(false);

}

// Regression test: a cover could be saved to the same file while the user was asked, and then the new cover was deleted, which the user wasn't asked about.
// The content of the file is compared, a file with the same content and another modification time is the same cover.
TEST_F(AlbumCoverChoiceControllerTest, DeleteCoverKeepsFileChangedWhileAsking) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString album_cover = temp_dir.filePath(u"cover.jpg"_s);

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  struct Change {
    const char *description;
    std::function<bool()> change;
    bool cover_changed;
  };
  const QList<Change> changes = {
    {"Another size", [&album_cover]() {
      QFile file(album_cover);
      return file.open(QIODevice::Append) && file.write("new") == 3;
    }, true},
    // Like on file systems where the modification time is only in seconds.
    {"The same size and modification time", [&album_cover]() {
      const QDateTime modified = QFileInfo(album_cover).lastModified();
      QFile file(album_cover);
      if (!file.open(QIODevice::ReadWrite) || !file.seek(file.size() / 2)) return false;
      const QByteArray byte = file.peek(1);
      if (byte.size() != 1) return false;
      // Flushed before setting the modification time, writing the data changes it.
      return file.write(QByteArray(1, static_cast<char>(byte.at(0) ^ 0x5A))) == 1 && file.flush() && file.setFileTime(modified, QFileDevice::FileModificationTime);
    }, true},
    {"The same content", [&album_cover]() {
      QFile file(album_cover);
      return file.open(QIODevice::ReadWrite) && file.setFileTime(file.fileTime(QFileDevice::FileModificationTime).addSecs(10), QFileDevice::FileModificationTime);
    }, false},
  };
  for (const Change &change : changes) {
    SCOPED_TRACE(change.description);
    ASSERT_TRUE(SaveImageFile(album_cover));
    Song song(Song::Source::Collection);
    song.set_url(QUrl::fromLocalFile(temp_dir.filePath(u"song.flac"_s)));
    song.set_albumartist(u"Artist"_s);
    song.set_album(u"Album"_s);
    song.set_art_automatic(QUrl::fromLocalFile(album_cover));
    std::optional<ConfirmedCoverFiles> confirmed_files;
    {
      bool changed = false;
      DeleteConfirmationAnswer answer(QDialogButtonBox::Yes, [&change, &changed]() { changed = change.change(); });
      confirmed_files = controller.ConfirmDeleteCover(SongList() << song);
      ASSERT_EQ(1, answer.shown());
      ASSERT_TRUE(changed);
    }
    ASSERT_TRUE(confirmed_files.has_value());
    // The cover is kept when the file changed.
    EXPECT_EQ(!change.cover_changed, controller.DeleteCover(&song, false, *confirmed_files));
    EXPECT_EQ(change.cover_changed, QFile::exists(album_cover));
    EXPECT_EQ(change.cover_changed, song.art_automatic().isValid());
  }

  database_->Close();

}

#ifndef Q_OS_WIN
// A cover file which can't be read can't be compared, so it's not handled as unchanged, and the cover is not deleted.
TEST_F(AlbumCoverChoiceControllerTest, DeleteCoverKeepsFileWhichCantBeRead) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString album_cover = temp_dir.filePath(u"cover.jpg"_s);
  ASSERT_TRUE(SaveImageFile(album_cover));
  ASSERT_TRUE(QFile::setPermissions(album_cover, QFileDevice::Permissions()));
  QFile file(album_cover);
  if (file.open(QIODevice::ReadOnly)) {
    GTEST_SKIP() << "The file can be read without permissions, like when running as root.";
  }

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  Song song(Song::Source::Collection);
  song.set_url(QUrl::fromLocalFile(temp_dir.filePath(u"song.flac"_s)));
  song.set_albumartist(u"Artist"_s);
  song.set_album(u"Album"_s);
  song.set_art_automatic(QUrl::fromLocalFile(album_cover));
  std::optional<ConfirmedCoverFiles> confirmed_files;
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    confirmed_files = controller.ConfirmDeleteCover(SongList() << song);
    ASSERT_EQ(1, answer.shown());
  }
  ASSERT_TRUE(confirmed_files.has_value());
  EXPECT_FALSE(controller.DeleteCover(&song, false, *confirmed_files));
  EXPECT_TRUE(QFile::exists(album_cover));

  database_->Close();

}
#endif

// Songs can share a cover file, and the cover of each song can be confirmed separately, then the file is compared with the file confirmed for each of the songs.
TEST_F(AlbumCoverChoiceControllerTest, CanDeleteCoversComparesSharedFileForEachSong) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString album_cover = temp_dir.filePath(u"cover.jpg"_s);
  ASSERT_TRUE(SaveImageFile(album_cover));

  AlbumCoverChoiceController controller;
  controller.Init(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader_, make_shared<CurrentAlbumCoverLoader>(albumcover_loader_), make_shared<CoverProviders>(), make_shared<StreamingServices>());

  SongList songs;
  for (const QString &name : {u"song1.flac"_s, u"song2.flac"_s}) {
    Song song(Song::Source::Collection);
    song.set_url(QUrl::fromLocalFile(temp_dir.filePath(name)));
    song.set_albumartist(u"Artist"_s);
    song.set_album(name);
    song.set_art_manual(QUrl::fromLocalFile(album_cover));
    songs << song;
  }

  const ConfirmedCoverFiles old_confirmed_files(QStringList() << album_cover);
  {
    QFile file(album_cover);
    ASSERT_TRUE(file.open(QIODevice::Append));
    ASSERT_EQ(3, file.write("new"));
  }
  const ConfirmedCoverFiles new_confirmed_files(QStringList() << album_cover);

  using SongConfirmedCoverFiles = std::pair<Song, ConfirmedCoverFiles>;
  EXPECT_TRUE(controller.CanDeleteCovers(QList<SongConfirmedCoverFiles>() << SongConfirmedCoverFiles(songs.value(0), new_confirmed_files) << SongConfirmedCoverFiles(songs.value(1), new_confirmed_files)));
  EXPECT_FALSE(controller.CanDeleteCovers(QList<SongConfirmedCoverFiles>() << SongConfirmedCoverFiles(songs.value(0), new_confirmed_files) << SongConfirmedCoverFiles(songs.value(1), old_confirmed_files)));
  EXPECT_FALSE(controller.CanDeleteCovers(QList<SongConfirmedCoverFiles>() << SongConfirmedCoverFiles(songs.value(0), old_confirmed_files) << SongConfirmedCoverFiles(songs.value(1), new_confirmed_files)));
  EXPECT_TRUE(QFile::exists(album_cover));

  database_->Close();

}

// Returns the contents of the file, or an empty byte array when it can't be read.
QByteArray ReadFile(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) return QByteArray();
  return file.readAll();
}

// Answers the message boxes with the button while it exists, and counts them.
class MessageBoxAnswer {
 public:
  explicit MessageBoxAnswer(const QMessageBox::StandardButton button) : shown_(0) {
    timer_.setInterval(10);
    QObject::connect(&timer_, &QTimer::timeout, &timer_, [this, button]() {
      QMessageBox *message_box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!message_box || !message_box->button(button)) return;
      ++shown_;
      message_box->button(button)->click();
    });
    timer_.start();
  }

  int shown() const { return shown_; }

 private:
  QTimer timer_;
  int shown_;
};

// Returns a cover with the image and the JPEG data of it.
AlbumCoverImageResult JpegCoverResult(const QColor &color) {
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(color);
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "JPG");
  return AlbumCoverImageResult(QUrl(), u"image/jpeg"_s, data, image);
}

// Returns the format of the image in the file, or an empty string when it's not an image.
QByteArray ImageFormat(const QString &filename) {
  return QImageReader(filename).format();
}

// Regression test: ".jpg" was added to a file name without an extension after the file dialog asked about overwriting the file, so an existing file with the extension was overwritten without asking.
TEST_F(AlbumCoverChoiceControllerTest, SaveCoverAsksBeforeOverwritingFileWithAddedExtension) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString existing_file = temp_dir.filePath(u"cover.jpg"_s);
  ASSERT_TRUE(SaveImageFile(existing_file));
  const QByteArray existing_data = ReadFile(existing_file);
  ASSERT_FALSE(existing_data.isEmpty());

  AlbumCoverChoiceController controller;
  const AlbumCoverImageResult result = JpegCoverResult(Qt::blue);

  {
    MessageBoxAnswer answer(QMessageBox::No);
    EXPECT_FALSE(controller.SaveCoverToChosenFile(temp_dir.filePath(u"cover"_s), result));
    EXPECT_EQ(1, answer.shown());
  }
  EXPECT_EQ(existing_data, ReadFile(existing_file));

  {
    MessageBoxAnswer answer(QMessageBox::Yes);
    EXPECT_TRUE(controller.SaveCoverToChosenFile(temp_dir.filePath(u"cover"_s), result));
    EXPECT_EQ(1, answer.shown());
  }
  EXPECT_EQ(result.image_data, ReadFile(existing_file));

  // Without an existing file, it's saved without asking.
  {
    MessageBoxAnswer answer(QMessageBox::No);
    EXPECT_TRUE(controller.SaveCoverToChosenFile(temp_dir.filePath(u"new-cover"_s), result));
    EXPECT_EQ(0, answer.shown());
  }
  EXPECT_EQ(result.image_data, ReadFile(temp_dir.filePath(u"new-cover.jpg"_s)));

}

// Regression test: the extension of an image format which can't be written was replaced with ".jpg" after the file dialog asked about overwriting the file, so an existing JPEG file was overwritten without asking.
TEST_F(AlbumCoverChoiceControllerTest, SaveCoverKeepsChosenFileWithUnsupportedExtension) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString existing_file = temp_dir.filePath(u"cover.jpg"_s);
  ASSERT_TRUE(SaveImageFile(existing_file));
  const QByteArray existing_data = ReadFile(existing_file);

  AlbumCoverChoiceController controller;
  const AlbumCoverImageResult result = JpegCoverResult(Qt::blue);

  MessageBoxAnswer answer(QMessageBox::No);
  const QString chosen_file = temp_dir.filePath(u"cover.unsupported-format"_s);
  EXPECT_TRUE(controller.SaveCoverToChosenFile(chosen_file, result));
  EXPECT_EQ(0, answer.shown());
  EXPECT_EQ(existing_data, ReadFile(existing_file));
  // The cover is saved as JPEG to the chosen file.
  EXPECT_EQ(result.image_data, ReadFile(chosen_file));

}

// The format is chosen by the last extension, a file name with more dots was taken as having an unsupported extension.
TEST_F(AlbumCoverChoiceControllerTest, SaveCoverUsesLastExtension) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  AlbumCoverChoiceController controller;
  const QString chosen_file = temp_dir.filePath(u"cover.v2.png"_s);
  EXPECT_TRUE(controller.SaveCoverToChosenFile(chosen_file, JpegCoverResult(Qt::blue)));
  EXPECT_EQ("png", ImageFormat(chosen_file));
  EXPECT_FALSE(QFile::exists(temp_dir.filePath(u"cover.v2.jpg"_s)));

}

}  // namespace
