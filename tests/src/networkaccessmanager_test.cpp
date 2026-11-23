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

#include "gtest_include.h"

#include <QObject>
#include <QList>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInformation>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTest>

#include "core/networkaccessmanager.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// Replies to /fast at once, and to /slow when Release() is called, keeping the connections alive.
class FakeHttpServer : public QObject {
 public:
  explicit FakeHttpServer(QObject *parent = nullptr) : QObject(parent), server_(new QTcpServer(this)) {
    QObject::connect(server_, &QTcpServer::newConnection, this, [this]() { NewConnection(); });
  }

  bool Listen() { return server_->listen(QHostAddress::LocalHost); }
  QUrl Url(const QString &path) const { return QUrl(u"http://127.0.0.1:%1%2"_s.arg(server_->serverPort()).arg(path)); }

  // The sockets of the connections still open.
  QList<QTcpSocket*> OpenConnections() const {
    QList<QTcpSocket*> sockets;
    for (const QPointer<QTcpSocket> &socket : connections_) {
      if (socket && socket->state() == QAbstractSocket::ConnectedState) sockets << socket;
    }
    return sockets;
  }

  bool slow_request_received() const { return slow_socket_ != nullptr; }

  void Release() {
    if (slow_socket_) Reply(slow_socket_);
    slow_socket_ = nullptr;
  }

 private:
  void NewConnection() {
    while (QTcpSocket *socket = server_->nextPendingConnection()) {
      connections_ << socket;
      QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { ReadyRead(socket); });
    }
  }

  void ReadyRead(QTcpSocket *socket) {
    QByteArray &data = buffers_[socket];
    data.append(socket->readAll());
    const qint64 end = data.indexOf("\r\n\r\n");
    if (end < 0) return;
    const QByteArray request_line = data.left(data.indexOf("\r\n"));
    data.remove(0, end + 4);
    if (request_line.contains(" /slow ")) {
      slow_socket_ = socket;
    }
    else {
      Reply(socket);
    }
  }

  static void Reply(QTcpSocket *socket) {
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nContent-Length: 4\r\nConnection: keep-alive\r\n\r\ndone");
  }

  QTcpServer *server_;
  QList<QPointer<QTcpSocket>> connections_;
  QHash<QTcpSocket*, QByteArray> buffers_;
  QPointer<QTcpSocket> slow_socket_;
};

class NetworkAccessManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    QStandardPaths::setTestModeEnabled(true);
    ASSERT_TRUE(server_.Listen());
  }

  QNetworkReply *Get(const QString &path) {
    QNetworkRequest network_request(server_.Url(path));
    network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    return network_.get(network_request);
  }

  void Online() {
    ASSERT_TRUE(QMetaObject::invokeMethod(&network_, "ReachabilityChanged", Qt::DirectConnection, Q_ARG(QNetworkInformation::Reachability, QNetworkInformation::Reachability::Online)));
  }

  static bool WaitForFinished(QNetworkReply *reply) {
    if (reply->isFinished()) return true;
    QSignalSpy spy(reply, &QNetworkReply::finished);
    return spy.wait(5000);
  }

  FakeHttpServer server_;
  NetworkAccessManager network_;
};

TEST_F(NetworkAccessManagerTest, OnlineClosesIdleConnections) {

  QNetworkReply *reply = Get(u"/fast"_s);
  ASSERT_TRUE(WaitForFinished(reply));
  EXPECT_EQ(QNetworkReply::NoError, reply->error());
  reply->deleteLater();
  ASSERT_EQ(1, server_.OpenConnections().size());

  Online();

  EXPECT_TRUE(QTest::qWaitFor([this]() { return server_.OpenConnections().isEmpty(); }, 5000));

  // The connections of later requests are kept.
  reply = Get(u"/fast"_s);
  ASSERT_TRUE(WaitForFinished(reply));
  EXPECT_EQ(QNetworkReply::NoError, reply->error());
  delete reply;
  QTest::qWait(100);
  EXPECT_EQ(1, server_.OpenConnections().size());

}

TEST_F(NetworkAccessManagerTest, OnlineDoesNotCancelRunningRequest) {

  QNetworkReply *reply = Get(u"/slow"_s);
  ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.slow_request_received(); }, 5000));

  Online();
  QTest::qWait(100);
  EXPECT_FALSE(reply->isFinished());
  EXPECT_EQ(1, server_.OpenConnections().size());

  server_.Release();
  ASSERT_TRUE(WaitForFinished(reply));
  EXPECT_EQ(QNetworkReply::NoError, reply->error()) << reply->errorString().toStdString();
  EXPECT_EQ("done", reply->readAll());

  // The connections are closed when the request has finished.
  EXPECT_TRUE(QTest::qWaitFor([this]() { return server_.OpenConnections().isEmpty(); }, 5000));
  delete reply;

}

// A request finishing while another request is running doesn't clear the cache, which would cancel the other request.
TEST_F(NetworkAccessManagerTest, OnlineWaitsForAllRunningRequests) {

  QNetworkReply *slow_reply = Get(u"/slow"_s);
  QNetworkReply *fast_reply = Get(u"/fast"_s);
  Online();

  ASSERT_TRUE(WaitForFinished(fast_reply));
  EXPECT_EQ(QNetworkReply::NoError, fast_reply->error()) << fast_reply->errorString().toStdString();
  ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.slow_request_received(); }, 5000));
  QTest::qWait(100);
  EXPECT_FALSE(slow_reply->isFinished());
  EXPECT_EQ(2, server_.OpenConnections().size());

  server_.Release();
  ASSERT_TRUE(WaitForFinished(slow_reply));
  EXPECT_EQ(QNetworkReply::NoError, slow_reply->error()) << slow_reply->errorString().toStdString();
  EXPECT_EQ("done", slow_reply->readAll());

  // The connections are closed when the last request has finished.
  EXPECT_TRUE(QTest::qWaitFor([this]() { return server_.OpenConnections().isEmpty(); }, 5000));
  delete fast_reply;
  delete slow_reply;

}

// QNetworkAccessManager's own connections to the reply are kept, so it still emits finished and deletes the replies automatically.
TEST_F(NetworkAccessManagerTest, ManagerFinishedAndAutoDeleteWork) {

  network_.setAutoDeleteReplies(true);
  QSignalSpy finished_spy(&network_, &QNetworkAccessManager::finished);
  QPointer<QNetworkReply> reply = Get(u"/fast"_s);
  QSignalSpy destroyed_spy(reply.data(), &QObject::destroyed);

  ASSERT_TRUE(finished_spy.wait(5000));
  EXPECT_EQ(1, finished_spy.count());
  EXPECT_TRUE(destroyed_spy.wait(5000));
  EXPECT_TRUE(reply.isNull());

}

// A request started when a request finishes, for example for a redirect, doesn't postpone clearing the cache.
TEST_F(NetworkAccessManagerTest, RequestStartedWhenFinishedDoesNotPostponeClearing) {

  QNetworkReply *reply = Get(u"/slow"_s);
  ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.slow_request_received(); }, 5000));
  const QList<QTcpSocket*> first_connections = server_.OpenConnections();
  ASSERT_EQ(1, first_connections.size());
  const QPointer<QTcpSocket> first_connection = first_connections.first();

  QNetworkReply *next_reply = nullptr;
  QObject::connect(reply, &QNetworkReply::finished, reply, [this, &next_reply]() { next_reply = Get(u"/slow"_s); });

  Online();
  server_.Release();
  ASSERT_TRUE(WaitForFinished(reply));
  EXPECT_EQ(QNetworkReply::NoError, reply->error()) << reply->errorString().toStdString();
  ASSERT_NE(nullptr, next_reply);

  // The connection of the finished request is closed while the next request is running.
  EXPECT_TRUE(QTest::qWaitFor([&first_connection]() { return !first_connection || first_connection->state() != QAbstractSocket::ConnectedState; }, 5000));
  ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.slow_request_received(); }, 5000));

  // The next request isn't cancelled.
  server_.Release();
  ASSERT_TRUE(WaitForFinished(next_reply));
  EXPECT_EQ(QNetworkReply::NoError, next_reply->error()) << next_reply->errorString().toStdString();
  EXPECT_EQ("done", next_reply->readAll());
  delete next_reply;
  delete reply;

}

TEST_F(NetworkAccessManagerTest, OnlineClosesConnectionsAfterDeletedRunningRequest) {

  QNetworkReply *slow_reply = Get(u"/slow"_s);
  ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.slow_request_received(); }, 5000));
  // Deleting a running reply doesn't emit finished.
  delete slow_reply;

  QNetworkReply *reply = Get(u"/fast"_s);
  ASSERT_TRUE(WaitForFinished(reply));
  EXPECT_EQ(QNetworkReply::NoError, reply->error());
  reply->deleteLater();
  ASSERT_FALSE(server_.OpenConnections().isEmpty());

  Online();

  EXPECT_TRUE(QTest::qWaitFor([this]() { return server_.OpenConnections().isEmpty(); }, 5000));

}

TEST(NetworkAccessManagerDeleteTest, DeleteWithRunningRequest) {

  QStandardPaths::setTestModeEnabled(true);
  FakeHttpServer server;
  ASSERT_TRUE(server.Listen());

  {
    NetworkAccessManager network;
    QNetworkReply *reply = network.get(QNetworkRequest(server.Url(u"/slow"_s)));
    ASSERT_TRUE(QTest::qWaitFor([&server]() { return server.slow_request_received(); }, 5000));
    EXPECT_FALSE(reply->isFinished());
  }

  // The manager deletes the running reply, which must not call back into the destroyed manager.
  EXPECT_TRUE(QTest::qWaitFor([&server]() { return server.OpenConnections().isEmpty(); }, 5000));

}

// The manager deletes the finished replies it still owns, which must not call back into the destroyed manager.
TEST(NetworkAccessManagerDeleteTest, DeleteWithFinishedReply) {

  QStandardPaths::setTestModeEnabled(true);
  FakeHttpServer server;
  ASSERT_TRUE(server.Listen());

  NetworkAccessManager network;
  QNetworkReply *reply = network.get(QNetworkRequest(server.Url(u"/fast"_s)));
  QSignalSpy spy(reply, &QNetworkReply::finished);
  ASSERT_TRUE(spy.wait(5000));
  EXPECT_EQ(QNetworkReply::NoError, reply->error());

}

TEST_F(NetworkAccessManagerTest, OtherReachabilityDoesNotCloseConnections) {

  QNetworkReply *reply = Get(u"/fast"_s);
  ASSERT_TRUE(WaitForFinished(reply));
  reply->deleteLater();
  ASSERT_EQ(1, server_.OpenConnections().size());

  ASSERT_TRUE(QMetaObject::invokeMethod(&network_, "ReachabilityChanged", Qt::DirectConnection, Q_ARG(QNetworkInformation::Reachability, QNetworkInformation::Reachability::Local)));
  QTest::qWait(200);

  EXPECT_EQ(1, server_.OpenConnections().size());

}

}  // namespace
