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

#include "gtest_include.h"

#include <QCoreApplication>
#include <QImage>
#include <QColor>
#include <QSignalSpy>
#include <QUrl>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <QFile>
#include <QSize>
#include <QBuffer>
#include <QTemporaryDir>
#include <QHash>
#include <QByteArray>

#include "includes/shared_ptr.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

QImage Image() {
  QImage image(16, 16, QImage::Format_RGB32);
  image.fill(QColor(Qt::red));
  return image;
}

template<typename Condition>
bool WaitFor(Condition condition) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < 10000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  return condition();
}

void ProcessEventsFor(const int msec) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < msec) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}

TEST(AlbumCoverLoaderTest, LoadsImage) {

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  const quint64 id = loader.LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage), Image());
  ASSERT_TRUE(WaitFor([&spy_loaded]() { return spy_loaded.count() > 0; }));
  EXPECT_EQ(id, spy_loaded.at(0).at(0).toULongLong());

}

// When exiting, the receivers can be deleted, so tasks not processed yet, and tasks added after exiting are not processed.
TEST(AlbumCoverLoaderTest, TasksNotProcessedAfterExit) {

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  QSignalSpy spy_exit_finished(&loader, &AlbumCoverLoader::ExitFinished);

  loader.LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage), Image());
  loader.ExitAsync();
  loader.LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage), Image());
  ASSERT_TRUE(WaitFor([&spy_exit_finished]() { return spy_exit_finished.count() > 0; }));
  loader.LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage), Image());

  ProcessEventsFor(200);
  EXPECT_EQ(0, spy_loaded.count());

}

// Remote covers still loading when exiting are aborted, and not finished.
TEST(AlbumCoverLoaderTest, RemoteCoverAbortedWhenExiting) {

  // The server accepts the request, but never replies.
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  QTcpSocket *socket = nullptr;
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &socket]() { socket = server.nextPendingConnection(); });

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  QSignalSpy spy_exit_finished(&loader, &AlbumCoverLoader::ExitFinished);

  const QUrl cover_url(QStringLiteral("http://127.0.0.1:%1/cover.jpg").arg(server.serverPort()));
  loader.LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage, QSize(32, 32), 1.0, AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual), false, QUrl(), cover_url, false);
  ASSERT_TRUE(WaitFor([&socket]() { return socket && socket->bytesAvailable() > 0; }));
  QSignalSpy spy_disconnected(socket, &QTcpSocket::disconnected);

  loader.ExitAsync();
  ASSERT_TRUE(WaitFor([&spy_exit_finished]() { return spy_exit_finished.count() > 0; }));
  EXPECT_TRUE(WaitFor([&spy_disconnected]() { return spy_disconnected.count() > 0; }));
  EXPECT_EQ(0, spy_loaded.count());

}

// Regression test: cancelling a task only removed it from the queue, a remote cover already being loaded was still loaded, and the task finished.
TEST(AlbumCoverLoaderTest, CancelledRemoteCoverIsAborted) {

  // The server accepts the requests, but never replies.
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  QList<QTcpSocket*> sockets;
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &sockets]() {
    while (server.hasPendingConnections()) sockets << server.nextPendingConnection();
  });

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);

  // The cancelled task would load the default cover after the remote cover failed.
  const QString default_cover = u":/pictures/cdcase.png"_s;
  ASSERT_TRUE(QFile::exists(default_cover));
  AlbumCoverLoaderOptions options(AlbumCoverLoaderOptions::Option::OriginalImage, QSize(32, 32), 1.0, AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual);
  options.default_cover = default_cover;
  const quint64 cancelled_id = loader.LoadImageAsync(options, false, QUrl(), QUrl(u"http://127.0.0.1:%1/cancelled.jpg"_s.arg(server.serverPort())), false);
  const quint64 other_id = loader.LoadImageAsync(options, false, QUrl(), QUrl(u"http://127.0.0.1:%1/other.jpg"_s.arg(server.serverPort())), false);
  ASSERT_TRUE(WaitFor([&sockets]() { return sockets.count() == 2 && sockets.at(0)->bytesAvailable() > 0 && sockets.at(1)->bytesAvailable() > 0; }));
  QTcpSocket *cancelled_socket = sockets.at(0)->peek(sockets.at(0)->bytesAvailable()).contains("cancelled.jpg") ? sockets.at(0) : sockets.at(1);
  QTcpSocket *other_socket = cancelled_socket == sockets.at(0) ? sockets.at(1) : sockets.at(0);
  QSignalSpy spy_cancelled_disconnected(cancelled_socket, &QTcpSocket::disconnected);
  QSignalSpy spy_other_disconnected(other_socket, &QTcpSocket::disconnected);

  loader.CancelTask(cancelled_id);
  EXPECT_TRUE(WaitFor([&spy_cancelled_disconnected]() { return spy_cancelled_disconnected.count() > 0; }));
  ProcessEventsFor(200);
  EXPECT_EQ(0, spy_loaded.count());

  // The other task is still loading.
  EXPECT_EQ(0, spy_other_disconnected.count());
  other_socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  ASSERT_TRUE(WaitFor([&spy_loaded]() { return spy_loaded.count() > 0; }));
  EXPECT_EQ(1, spy_loaded.count());
  EXPECT_EQ(other_id, spy_loaded.at(0).at(0).toULongLong());

}

// Replies to the requests for a path with a redirect to another path, and to the requests for cover.png with the image.
class RedirectingServer : public QTcpServer {
 public:
  explicit RedirectingServer(QObject *parent = nullptr) : QTcpServer(parent), requests_(0) {
    QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
      while (QTcpSocket *socket = nextPendingConnection()) {
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() { ReadyRead(socket); });
      }
    });
  }

  QUrl Url(const QString &path) const { return QUrl(u"http://127.0.0.1:%1%2"_s.arg(serverPort()).arg(path)); }
  void AddRedirect(const QByteArray &path, const QByteArray &location) { redirects_.insert(path, location); }
  int requests() const { return requests_; }

 private:
  void ReadyRead(QTcpSocket *socket) {
    QByteArray &request = buffers_[socket];
    request.append(socket->readAll());
    while (request.contains("\r\n\r\n")) {
      const QByteArray path = request.split(' ').value(1);
      request.remove(0, request.indexOf("\r\n\r\n") + 4);
      ++requests_;
      if (redirects_.contains(path)) {
        socket->write("HTTP/1.1 302 Found\r\nLocation: " + redirects_.value(path) + "\r\nContent-Length: 0\r\n\r\n");
      }
      else if (path == "/cover.png") {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        Image().save(&buffer, "PNG");
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\n\r\n" + data);
      }
      else {
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
      }
    }
  }

  QHash<QByteArray, QByteArray> redirects_;
  QHash<QTcpSocket*, QByteArray> buffers_;
  int requests_;
};

AlbumCoverLoaderOptions ManualCoverOptions() {
  return AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::OriginalImage, QSize(32, 32), 1.0, AlbumCoverLoaderOptions::Types() << AlbumCoverLoaderOptions::Type::Manual);
}

// A redirected cover is loaded from the URL it's redirected to.
TEST(AlbumCoverLoaderTest, RedirectedRemoteCoverIsLoaded) {

  RedirectingServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.AddRedirect("/redirect.png", "/cover.png");

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  loader.LoadImageAsync(ManualCoverOptions(), false, QUrl(), server.Url(u"/redirect.png"_s), false);
  ASSERT_TRUE(WaitFor([&spy_loaded]() { return spy_loaded.count() > 0; }));

  const AlbumCoverLoaderResult result = spy_loaded.at(0).at(1).value<AlbumCoverLoaderResult>();
  EXPECT_TRUE(result.success);
  EXPECT_EQ(Image().size(), result.album_cover.image.size());
  EXPECT_EQ(2, server.requests());

}

// A redirect to another scheme is not followed, so a server can't redirect to a local file, which would be loaded as the cover.
TEST(AlbumCoverLoaderTest, UnsafeRedirectIsNotFollowed) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"local.png"_s);
  ASSERT_TRUE(Image().save(filename, "PNG"));

  RedirectingServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.AddRedirect("/redirect.png", QUrl::fromLocalFile(filename).toEncoded());

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  loader.LoadImageAsync(ManualCoverOptions(), false, QUrl(), server.Url(u"/redirect.png"_s), false);
  ASSERT_TRUE(WaitFor([&spy_loaded]() { return spy_loaded.count() > 0; }));

  EXPECT_FALSE(spy_loaded.at(0).at(1).value<AlbumCoverLoaderResult>().success);

}

// A redirect loop is stopped after a few redirects, instead of the 50 redirects QNetworkAccessManager follows by default.
TEST(AlbumCoverLoaderTest, RedirectLoopIsLimited) {

  RedirectingServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.AddRedirect("/loop.png", "/loop.png");

  AlbumCoverLoader loader(nullptr);
  QSignalSpy spy_loaded(&loader, &AlbumCoverLoader::AlbumCoverLoaded);
  loader.LoadImageAsync(ManualCoverOptions(), false, QUrl(), server.Url(u"/loop.png"_s), false);
  ASSERT_TRUE(WaitFor([&spy_loaded]() { return spy_loaded.count() > 0; }));

  EXPECT_FALSE(spy_loaded.at(0).at(1).value<AlbumCoverLoaderResult>().success);
  // The first request and 3 redirects.
  EXPECT_EQ(4, server.requests());

}

}  // namespace
