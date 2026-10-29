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

#include "includes/shared_ptr.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"

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

}  // namespace
