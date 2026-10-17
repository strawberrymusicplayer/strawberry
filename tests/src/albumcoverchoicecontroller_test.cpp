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

#include "gtest_include.h"

#include <QCoreApplication>
#include <QThread>
#include <QTemporaryDir>
#include <QFile>
#include <QString>
#include <QImage>
#include <QColor>
#include <QElapsedTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverimageresult.h"
#include "streaming/streamingservices.h"
#include "utilities/imageutils.h"

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
  bool HasEmbeddedCoverColor(const QString &filename, const QColor &color) const {
    QImage image;
    if (!tagreader_client_->LoadCoverImageBlocking(filename, image).success() || image.isNull()) return false;
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

}  // namespace
