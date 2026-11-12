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

#include <gst/gst.h>

#include "gtest_include.h"

#include <QByteArray>
#include <QFile>
#include <QFuture>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrentRun>
#include <QHostAddress>
#include <QList>
#include <QHash>
#include <QPointer>

#include "includes/shared_ptr.h"
#include "core/memorydatabase.h"
#include "core/songloader.h"
#include "core/urlhandlers.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

// Playlists which are not in the collection are parsed in a concurrent thread, the errors of the parser should be returned when it's finished.
TEST(SongLoaderTest, ParserErrorsFromConcurrentThread) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"playlist.m3u"_s);
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    // A URL with a scheme the parser doesn't know how to handle.
    file.write(QByteArray("unknownscheme://example/song.mp3\n"));
    file.close();
  }

  SharedPtr<Database> database = make_shared<MemoryDatabase>(nullptr);
  SharedPtr<CollectionBackend> collection_backend = make_shared<CollectionBackend>();
  collection_backend->Init(database, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));

  SongLoader loader(make_shared<UrlHandlers>(), collection_backend, nullptr);
  ASSERT_EQ(SongLoader::Result::BlockingLoadRequired, loader.Load(QUrl::fromLocalFile(filename)));

  // Load and read the errors in a concurrent thread, like SongLoaderInserter does.
  // Use a separate thread pool and wait for it, waiting for the future can run the task in this thread instead.
  QThreadPool threadpool;
  QFuture<QStringList> future = QtConcurrent::run(&threadpool, [&loader]() {
    loader.LoadFilenamesBlocking();
    return loader.errors();
  });
  threadpool.waitForDone();

  const QStringList errors = future.result();
  ASSERT_EQ(1, errors.count());
  EXPECT_TRUE(errors.first().contains(u"unknownscheme://example/song.mp3"_s));

}

constexpr int kLargePlaylistEntries = 1500;

// Replies to the requests by the path: not found, text which isn't a playlist, an empty reply, playlists, audio, or no reply.
class FakeHttpServer : public QTcpServer {
 public:
  explicit FakeHttpServer(QObject *parent = nullptr) : QTcpServer(parent) {
    QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
      while (QTcpSocket *socket = nextPendingConnection()) {
        sockets_ << socket;
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() { ReadyRead(socket); });
      }
    });
  }

  QUrl Url(const QString &path) const {
    return QUrl(u"http://127.0.0.1:%1%2"_s.arg(serverPort()).arg(path));
  }

 private:
  // The request can arrive in parts, so wait for the end of the headers.
  void ReadyRead(QTcpSocket *socket) {

    QByteArray &request = requests_[socket];
    request.append(socket->readAll());
    if (!request.contains("\r\n\r\n")) return;

    const QByteArray request_line = request.left(request.indexOf("\r\n"));
    request.clear();
    Reply(socket, request_line.split(' ').value(1));

  }

  static void Reply(QTcpSocket *socket, const QByteArray &path) {

    if (path.startsWith("/noreply")) return;

    QByteArray status = "200 OK";
    QByteArray data;
    if (path.startsWith("/notfound")) {
      status = "404 Not Found";
      data = "Not found\n";
    }
    else if (path.startsWith("/text")) {
      data = "Hello, this is some text which is not a playlist.\n";
    }
    else if (path.startsWith("/playlist")) {
      data = "#EXTM3U\nhttp://example.com/stream.mp3\n";
    }
    else if (path.startsWith("/largepls")) {
      // Larger than the data GStreamer's typefind looks at before it gives up, with Latin-1 titles, so it isn't recognised as UTF-8 text.
      data = "[playlist]\n";
      for (int i = 1; i <= kLargePlaylistEntries; ++i) {
        data += "File" + QByteArray::number(i) + "=http://example.com/" + QByteArray(64, 'a') + QByteArray::number(i) + ".mp3\n";
        data += "Title" + QByteArray::number(i) + "=Caf\xe9 " + QByteArray::number(i) + "\n";
      }
      data += "NumberOfEntries=" + QByteArray::number(kLargePlaylistEntries) + "\n";
    }
    else if (path.startsWith("/pls")) {
      // With a Latin-1 title, so it isn't recognised as UTF-8 text.
      data = "[playlist]\nFile1=http://example.com/stream.mp3\nTitle1=Caf\xe9\nNumberOfEntries=1\n";
    }
    else if (path.startsWith("/audio")) {
      QFile file(u":/audio/strawberry.flac"_s);
      if (file.open(QIODevice::ReadOnly)) data = file.readAll();
    }

    socket->write("HTTP/1.1 " + status + "\r\nContent-Type: text/plain\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data);
    socket->disconnectFromHost();

  }

  QList<QPointer<QTcpSocket>> sockets_;
  QHash<QTcpSocket*, QByteArray> requests_;
};

class SongLoaderRemoteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gst_init(nullptr, nullptr);
    ASSERT_TRUE(server_.listen(QHostAddress::LocalHost));
  }

  SongLoader::Result Load(const QUrl &url, QStringList *errors, SongList *songs = nullptr, const int timeout = 2000) {
    SongLoader loader(make_shared<UrlHandlers>(), nullptr, nullptr);
    loader.set_timeout(timeout);
    const SongLoader::Result result = loader.Load(url);
    if (result != SongLoader::Result::BlockingLoadRequired) {
      *errors = loader.errors();
      return result;
    }
    const SongLoader::Result blocking_result = loader.LoadFilenamesBlocking();
    *errors = loader.errors();
    if (songs) *songs = loader.songs();
    return blocking_result;
  }

  FakeHttpServer server_;
};

// Regression test: a remote URL which couldn't be loaded was loaded with success, without songs or errors, so nothing happened, and there was no error.
TEST_F(SongLoaderRemoteTest, NotFoundIsError) {

  QStringList errors;
  EXPECT_EQ(SongLoader::Result::Error, Load(server_.Url(u"/notfound/playlist.m3u?token=secret_token"_s), &errors));
  ASSERT_EQ(1, errors.count()) << errors.join(u'\n').toStdString();
  EXPECT_TRUE(errors.first().contains(server_.Url(u"/notfound/playlist.m3u"_s).toString())) << errors.first().toStdString();
  // The query can have credentials.
  EXPECT_FALSE(errors.first().contains(u"secret_token"_s)) << errors.first().toStdString();

}

TEST_F(SongLoaderRemoteTest, TextWhichIsNotPlaylistIsError) {

  QStringList errors;
  EXPECT_EQ(SongLoader::Result::Error, Load(server_.Url(u"/text"_s), &errors));
  ASSERT_EQ(1, errors.count()) << errors.join(u'\n').toStdString();
  EXPECT_TRUE(errors.first().contains(u"not a recognised playlist"_s)) << errors.first().toStdString();

}

TEST_F(SongLoaderRemoteTest, EmptyIsError) {

  QStringList errors;
  EXPECT_EQ(SongLoader::Result::Error, Load(server_.Url(u"/empty"_s), &errors));
  EXPECT_EQ(1, errors.count()) << errors.join(u'\n').toStdString();

}

TEST_F(SongLoaderRemoteTest, TimeoutIsError) {

  QStringList errors;
  EXPECT_EQ(SongLoader::Result::Error, Load(server_.Url(u"/noreply"_s), &errors, nullptr, 300));
  ASSERT_EQ(1, errors.count()) << errors.join(u'\n').toStdString();
  EXPECT_TRUE(errors.first().contains(u"Timed out"_s)) << errors.first().toStdString();

}

TEST_F(SongLoaderRemoteTest, PlaylistIsLoaded) {

  QStringList errors;
  SongList songs;
  EXPECT_EQ(SongLoader::Result::Success, Load(server_.Url(u"/playlist.m3u"_s), &errors, &songs));
  EXPECT_TRUE(errors.isEmpty()) << errors.join(u'\n').toStdString();
  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(QUrl(u"http://example.com/stream.mp3"_s), songs.first().url());

}

// Regression test: the data was captured after the typefind, which doesn't pass on the data when it can't find the type, so a playlist with a type GStreamer can't find timed out.
TEST_F(SongLoaderRemoteTest, PlaylistWithUnknownTypeIsLoaded) {

  QStringList errors;
  SongList songs;
  EXPECT_EQ(SongLoader::Result::Success, Load(server_.Url(u"/pls"_s), &errors, &songs));
  EXPECT_TRUE(errors.isEmpty()) << errors.join(u'\n').toStdString();
  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(QUrl(u"http://example.com/stream.mp3"_s), songs.first().url());

}

// Regression test: GStreamer's typefind gives up on finding the type before the end of a large stream, and stops the stream, so the playlist was loaded with only the data received so far, without the rest of the songs.
TEST_F(SongLoaderRemoteTest, LargePlaylistWithUnknownTypeIsError) {

  QStringList errors;
  SongList songs;
  EXPECT_EQ(SongLoader::Result::Error, Load(server_.Url(u"/largepls"_s), &errors, &songs));
  EXPECT_TRUE(songs.isEmpty()) << songs.count();
  ASSERT_EQ(1, errors.count()) << errors.join(u'\n').toStdString();
  EXPECT_TRUE(errors.first().contains(u"type is unknown"_s)) << errors.first().toStdString();

}

// Audio is added as a stream.
TEST_F(SongLoaderRemoteTest, AudioIsLoadedAsStream) {

  QStringList errors;
  SongList songs;
  const QUrl url = server_.Url(u"/audio.flac"_s);
  EXPECT_EQ(SongLoader::Result::Success, Load(url, &errors, &songs));
  EXPECT_TRUE(errors.isEmpty()) << errors.join(u'\n').toStdString();
  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(url, songs.first().url());

}

}  // namespace
