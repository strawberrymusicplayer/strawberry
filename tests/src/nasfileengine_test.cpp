// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "core/nasfileengine.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "collection/collectionwatcher.h"
#include "engine/gstqfilesrc.h"
#include "engine/gststartup.h"
#include "tagreader/taglibqfilestream.h"
#include "tagreader/tagreadertaglib.h"
#include "tagreader/tagreaderclient.h"
#include <taglib/fileref.h>
#include <thread>

namespace {

Nas::Location Location() {
  return {QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("127.0.0.1"), QStringLiteral("music"), QStringLiteral("album"), QStringLiteral("strawberry-test"), QStringLiteral("fixture-only"), QString()};
}

TEST(NasFileEngineTest, ValidatesPathsAndKeepsCredentialsOutOfLabels) {
  auto location = Location();
  EXPECT_TRUE(Nas::Validate(location).isEmpty());
  EXPECT_FALSE(location.label().contains(location.password));
  EXPECT_FALSE(location.label().contains(location.user));
  location.folder = QStringLiteral("../escape");
  EXPECT_FALSE(Nas::Validate(location).isEmpty());
  location = Location();
  location.server = QStringLiteral("smb://user:password@nas");
  EXPECT_FALSE(Nas::Validate(location).isEmpty());
  location = Location();
  location.share = QStringLiteral("music/other");
  EXPECT_FALSE(Nas::Validate(location).isEmpty());
  location = Location();
  location.user.clear();
  EXPECT_FALSE(Nas::Validate(location).isEmpty());
}

TEST(NasFileEngineTest, UnknownLocationsFailRatherThanAccessingLocalFiles) {
  Nas::Install();
  Nas::SetLocations({});
  const quint64 before = Nas::FailureSerial();
  QFile file(Location().path() + QStringLiteral("/track.wav"));
  EXPECT_FALSE(file.open(QIODevice::ReadOnly));
  EXPECT_FALSE(file.errorString().isEmpty());
  EXPECT_GT(Nas::FailureSerial(), before);
}

class NasProtocolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const QString python = qEnvironmentVariable("STRAWBERRY_NAS_TEST_PYTHON");
    if (python.isEmpty()) GTEST_SKIP() << "Set STRAWBERRY_NAS_TEST_PYTHON to an interpreter with impacket for loopback SMB2 integration tests";
    const QString script = QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath(QStringLiteral("../data/nas_server.py"));
    server_.start(python, {script});
    ASSERT_TRUE(server_.waitForStarted(5000));
    ASSERT_TRUE(server_.waitForReadyRead(10000)) << server_.readAllStandardError().constData();
    bool ok;
    const int port = server_.readLine().trimmed().toInt(&ok);
    ASSERT_TRUE(ok);
    ASSERT_GT(port, 0);
    location_ = Location();
    location_.server += u':' + QString::number(port);
    Nas::Install();
    Nas::SetLocations({location_});
  }
  void TearDown() override {
    Nas::SetLocations({});
    server_.closeWriteChannel();
    if (server_.state() != QProcess::NotRunning && !server_.waitForFinished(3000)) {
      server_.terminate();
      if (!server_.waitForFinished(3000)) { server_.kill(); server_.waitForFinished(3000); }
    }
  }
  QProcess server_;
  Nas::Location location_;

  enum class ScanEnd { Complete, Disconnect, Cancel, CancelCounting, CancelDuringCounting };
  void VerifyFolderPublishing(ScanEnd end) {
    const auto tasks = std::make_shared<TaskManager>();
    const auto database = std::make_shared<MemoryDatabase>(nullptr);
    const auto backend = std::make_shared<CollectionBackend>();
    backend->Init(database, tasks, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    backend->AddDirectory(location_.path());
    Song existing;
    existing.Init(QStringLiteral("Existing title"), QStringLiteral("Artist"), QStringLiteral("Album"), 1000000000);
    existing.set_source(Song::Source::Collection);
    existing.set_directory_id(1);
    existing.set_url(QUrl::fromLocalFile(location_.path() + QStringLiteral("/track.wav")));
    existing.set_filetype(Song::FileType::WAV);
    existing.set_samplerate(16000);
    Song missing = existing;
    missing.set_url(QUrl::fromLocalFile(location_.path() + QStringLiteral("/missing.wav")));
    backend->AddOrUpdateSongs({existing, missing});
    ASSERT_EQ(backend->GetAllSongs().size(), 2);

    const auto reader = std::make_shared<TagReaderClient>();
    CollectionWatcher watcher(Song::Source::Collection, tasks, reader, backend);
    QSignalSpy deleted(&watcher, &CollectionWatcher::SongsDeleted);
    QSignalSpy unavailable(&watcher, &CollectionWatcher::SongsUnavailable);
    QSignalSpy expiry(&watcher, &CollectionWatcher::UpdateLastSeen);
    QSignalSpy subdirs(&watcher, &CollectionWatcher::SubdirsDiscovered);
    int published = 0;
    int updated = 0;
    bool stopped_while_counting = false;
    const auto counting_connection = QObject::connect(&*tasks, &TaskManager::TasksChanged, &watcher, [&]() {
      if (end != ScanEnd::CancelDuringCounting || stopped_while_counting) return;
      const auto active = tasks->GetTasks();
      if (active.isEmpty() || !active.first().detail.startsWith(QStringLiteral("Counting files:"))) return;
      stopped_while_counting = true;
      EXPECT_EQ(active.first().progress_max, 0);
      tasks->RequestTaskCancellation(active.first().id);
    });
    const auto started_connection = QObject::connect(&watcher, &CollectionWatcher::ScanStarted, &watcher, [&](int id) {
      if (end == ScanEnd::CancelCounting) {
        const auto active = tasks->GetTasks();
        ASSERT_EQ(active.size(), 1);
        EXPECT_EQ(active.first().progress_max, 0);
        EXPECT_TRUE(active.first().detail.contains(QStringLiteral("Counting")));
        std::thread request([tasks, id]() { tasks->RequestTaskCancellation(id); });
        request.join();
      }
    });
    const auto published_connection = QObject::connect(&watcher, &CollectionWatcher::NewOrUpdatedSongs, &watcher, [&](const SongList &songs) {
      ASSERT_EQ(songs.size(), 1);
      if (songs.first().id() != -1) {
        ++updated;
        EXPECT_EQ(end, ScanEnd::Complete);
        EXPECT_EQ(published, 2);
      }
      else {
        ++published;
        // This is a live folder result, not a batch emitted by the transaction destructor.
        const QList<TaskManager::Task> active = tasks->GetTasks();
        ASSERT_EQ(active.size(), 1);
        if (published == 1) EXPECT_LT(active.first().progress, active.first().progress_max);
        EXPECT_EQ(deleted.count(), 0);
        EXPECT_EQ(unavailable.count(), 0);
        EXPECT_EQ(expiry.count(), 0);
        EXPECT_EQ(subdirs.count(), 0);
        for (const Song &song : backend->GetAllSongs()) {
          if (song.url() == existing.url()) EXPECT_EQ(song.samplerate(), 16000);
        }
      }
      backend->AddOrUpdateSongs(songs);
      EXPECT_EQ(backend->GetAllSongs().size(), 2 + published);
      if (published == 1 && end == ScanEnd::Disconnect) {
        server_.terminate();
        ASSERT_TRUE(server_.waitForFinished(3000));
      }
      else if (published == 1 && end == ScanEnd::Cancel) {
        tasks->RequestTaskCancellation(tasks->GetTasks().first().id);
      }
    });
    CollectionDirectory directory;
    directory.id = 1;
    directory.path = location_.path();
    watcher.AddDirectory(directory, {});
    EXPECT_TRUE(tasks->GetTasks().isEmpty());
    const bool stopped_counting = end == ScanEnd::CancelCounting || end == ScanEnd::CancelDuringCounting;
    EXPECT_EQ(published, end == ScanEnd::Complete ? 2 : (stopped_counting ? 0 : 1));
    EXPECT_EQ(updated, end == ScanEnd::Complete ? 1 : 0);
    EXPECT_EQ(deleted.count() + unavailable.count(), end == ScanEnd::Complete ? 1 : 0);
    EXPECT_EQ(subdirs.count(), end == ScanEnd::Complete ? 1 : 0);
    EXPECT_EQ(expiry.count(), 0);
    if (end == ScanEnd::CancelDuringCounting) EXPECT_TRUE(stopped_while_counting);
    if (end == ScanEnd::Cancel || stopped_counting) {
      QObject::disconnect(started_connection);
      QObject::disconnect(published_connection);
      QObject::disconnect(counting_connection);
      QObject::connect(&watcher, &CollectionWatcher::NewOrUpdatedSongs, &*backend, &CollectionBackend::AddOrUpdateSongs);
      watcher.AddDirectory(directory, {});
      EXPECT_EQ(backend->GetAllSongs().size(), 4);
      EXPECT_TRUE(tasks->GetTasks().isEmpty());
    }
  }
};

TEST_F(NasProtocolTest, ListsReadsSeeksAndRejectsWrites) {
  ASSERT_TRUE(Nas::Check(location_).isEmpty());
  EXPECT_TRUE(QFileInfo(location_.path()).isDir());
  const QStringList entries = QDir(location_.path()).entryList(QDir::Files | QDir::NoDotAndDotDot);
  EXPECT_TRUE(entries.contains(QStringLiteral("track.wav")));
  EXPECT_TRUE(entries.contains(QStringLiteral("bytes.bin")));
  EXPECT_EQ(QDir(location_.path()).entryList({QStringLiteral("*.wav")}, QDir::Files), QStringList{QStringLiteral("track.wav")});
  QFile file(location_.path() + QStringLiteral("/bytes.bin"));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << file.errorString().toStdString();
  EXPECT_EQ(file.size(), 2097152);
  EXPECT_EQ(file.read(3), QByteArray::fromHex("000102"));
  ASSERT_TRUE(file.seek(123456));
  EXPECT_EQ(file.read(3), QByteArray::fromHex("404142"));
  ASSERT_TRUE(file.seek(0));
  const QByteArray all = file.readAll();
  ASSERT_EQ(all.size(), 2097152);
  EXPECT_EQ(all.mid(65535, 3), QByteArray::fromHex("ff0001"));
  file.close();
  EXPECT_FALSE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  EXPECT_FALSE(QFile::remove(file.fileName()));
  EXPECT_EQ(QFileInfo(file.fileName()).size(), 2097152);
  const quint64 before = Nas::FailureSerial();
  EXPECT_FALSE(QFileInfo(location_.path() + QStringLiteral("/missing.wav")).exists());
  EXPECT_EQ(Nas::FailureSerial(), before);
  EXPECT_TRUE(Nas::Check(location_).isEmpty());
  auto root = location_;
  root.folder.clear();
  Nas::SetLocations({root});
  EXPECT_TRUE(QDir(root.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty());
  EXPECT_EQ(QDir(root.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{QStringLiteral("album")});
}

TEST_F(NasProtocolTest, TagLibAndGStreamerReadNetworkWav) {
  const QString path = location_.path() + QStringLiteral("/track.wav");
  TagReaderTagLib reader;
  Song song;
  const TagReaderResult result = reader.ReadFile(path, &song);
  ASSERT_TRUE(result.success()) << result.error_string().toStdString();
  EXPECT_EQ(song.samplerate(), 8000);
  TagLibQFileStream stream(path);
  ASSERT_TRUE(stream.isOpen());
  TagLib::FileRef ref(&stream);
  ASSERT_FALSE(ref.isNull());
  ASSERT_NE(ref.audioProperties(), nullptr);
  EXPECT_EQ(ref.audioProperties()->sampleRate(), 8000);
  GstStartup::Initialize();
  ASSERT_TRUE(RegisterGstQFileSource());
  GError *error = nullptr;
  GstDiscoverer *discoverer = gst_discoverer_new(20 * GST_SECOND, &error);
  ASSERT_NE(discoverer, nullptr);
  GstDiscovererInfo *info = gst_discoverer_discover_uri(discoverer, QUrl::fromLocalFile(path).toEncoded().constData(), &error);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(gst_discoverer_info_get_result(info), GST_DISCOVERER_OK);
  EXPECT_EQ(gst_discoverer_info_get_duration(info), GST_SECOND);
  gst_discoverer_info_unref(info);
  gst_object_unref(discoverer);
  if (error) { ADD_FAILURE() << error->message; g_error_free(error); }
}

TEST_F(NasProtocolTest, CollectionScannerDiscoversNetworkTracks) {
  const auto tasks = std::make_shared<TaskManager>();
  const auto database = std::make_shared<MemoryDatabase>(nullptr);
  const auto backend = std::make_shared<CollectionBackend>();
  backend->Init(database, tasks, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  const auto reader = std::make_shared<TagReaderClient>();
  CollectionWatcher watcher(Song::Source::Collection, tasks, reader, backend);
  QSignalSpy discovered(&watcher, &CollectionWatcher::NewOrUpdatedSongs);
  CollectionDirectory directory;
  directory.id = 1;
  directory.path = location_.path();
  watcher.AddDirectory(directory, {});
  ASSERT_EQ(discovered.count(), 3);
  QSet<QUrl> urls;
  for (const QList<QVariant> &batch : discovered) {
    const SongList songs = batch.at(0).value<SongList>();
    ASSERT_EQ(songs.size(), 1);
    urls.insert(songs.at(0).url());
    EXPECT_EQ(songs.at(0).samplerate(), 8000);
  }
  EXPECT_EQ(urls, (QSet<QUrl>{QUrl::fromLocalFile(location_.path() + QStringLiteral("/track.wav")),
                            QUrl::fromLocalFile(location_.path() + QStringLiteral("/first/track.wav")),
                            QUrl::fromLocalFile(location_.path() + QStringLiteral("/second/track.wav"))}));
}

TEST_F(NasProtocolTest, WrongPasswordAndOfflineServerFailExplicitly) {
  auto bad = location_;
  bad.password = QStringLiteral("incorrect-fixture");
  EXPECT_FALSE(Nas::Check(bad).isEmpty());
  QFile interrupted(location_.path() + QStringLiteral("/bytes.bin"));
  ASSERT_TRUE(interrupted.open(QIODevice::ReadOnly));
  EXPECT_EQ(interrupted.read(3), QByteArray::fromHex("000102"));
  const quint64 before = Nas::FailureSerial();
  server_.terminate();
  ASSERT_TRUE(server_.waitForFinished(3000));
  interrupted.read(128 * 1024);
  EXPECT_NE(interrupted.error(), QFile::NoError);
  EXPECT_GT(Nas::FailureSerial(), before);
  QFile file(location_.path() + QStringLiteral("/track.wav"));
  EXPECT_FALSE(file.open(QIODevice::ReadOnly));
  EXPECT_GT(Nas::FailureSerial(), before);
}

TEST_F(NasProtocolTest, PublishesEachFolderBeforeFinishingAndDefersExistingChanges) {
  VerifyFolderPublishing(ScanEnd::Complete);
}

TEST_F(NasProtocolTest, DisconnectKeepsPublishedFoldersWithoutChangingExistingSongs) {
  VerifyFolderPublishing(ScanEnd::Disconnect);
}

TEST_F(NasProtocolTest, CancellationKeepsPublishedFoldersWithoutChangingExistingSongs) {
  VerifyFolderPublishing(ScanEnd::Cancel);
}

TEST_F(NasProtocolTest, CanStopBeforeCountingWithoutChangingExistingSongs) {
  VerifyFolderPublishing(ScanEnd::CancelCounting);
}

TEST_F(NasProtocolTest, CanStopDuringCountingWithoutChangingExistingSongs) {
  VerifyFolderPublishing(ScanEnd::CancelDuringCounting);
}

}  // namespace
