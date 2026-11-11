#include "gtest_include.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QStringList>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

#include "core/localredirectserver.h"

using namespace Qt::Literals::StringLiterals;

TEST(LocalRedirectServerTest, AcceptsProviderCallbacks) {

  const QStringList parameters = {u"code"_s, u"code_autorisation"_s, u"token"_s, u"error"_s, u"error_description"_s};
  for (const QString &parameter : parameters) {
    LocalRedirectServer server;
    ASSERT_TRUE(server.Listen());
    QSignalSpy finished(&server, &LocalRedirectServer::Finished);
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.port());
    ASSERT_TRUE(socket.waitForConnected());
    socket.write("GET /?" + parameter.toUtf8() + "=test-value HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ASSERT_TRUE(finished.wait(1000));
    EXPECT_EQ(1, finished.count());
    EXPECT_TRUE(server.success());
    EXPECT_FALSE(server.isListening());
    EXPECT_EQ(u"test-value"_s, QUrlQuery(server.request_url()).queryItemValue(parameter));
    ASSERT_TRUE(QTest::qWaitFor([&socket]() { return socket.bytesAvailable() > 0; }));
    EXPECT_TRUE(socket.readAll().startsWith("HTTP/1.0 200 OK"));
  }

}

TEST(LocalRedirectServerTest, IgnoresFaviconBeforeCallback) {

  LocalRedirectServer server;
  ASSERT_TRUE(server.Listen());
  QSignalSpy finished(&server, &LocalRedirectServer::Finished);
  QTcpSocket favicon;
  favicon.connectToHost(QHostAddress::LocalHost, server.port());
  ASSERT_TRUE(favicon.waitForConnected());
  favicon.write("GET /favicon.ico HTTP/1.1\r\nHost: localhost\r\n\r\n");
  ASSERT_TRUE(QTest::qWaitFor([&favicon]() { return favicon.bytesAvailable() > 0; }));
  EXPECT_TRUE(favicon.readAll().startsWith("HTTP/1.0 404 Not Found"));
  EXPECT_TRUE(finished.isEmpty());
  EXPECT_TRUE(server.isListening());

  QTcpSocket callback;
  callback.connectToHost(QHostAddress::LocalHost, server.port());
  ASSERT_TRUE(callback.waitForConnected());
  callback.write("GET /?code=test-code&state=test-state HTTP/1.1\r\nHost: localhost\r\n\r\n");
  ASSERT_TRUE(finished.wait(1000));
  EXPECT_TRUE(server.success());
  EXPECT_EQ(u"test-state"_s, QUrlQuery(server.request_url()).queryItemValue(u"state"_s));

}

TEST(LocalRedirectServerTest, WaitsForCompleteHeaders) {

  LocalRedirectServer server;
  ASSERT_TRUE(server.Listen());
  QSignalSpy finished(&server, &LocalRedirectServer::Finished);
  QTcpSocket socket;
  socket.connectToHost(QHostAddress::LocalHost, server.port());
  ASSERT_TRUE(socket.waitForConnected());
  socket.write("GET /?code=test-code HTTP/1.1\r\nHost: local");
  EXPECT_FALSE(finished.wait(50));
  EXPECT_TRUE(server.isListening());
  socket.write("host\r\n\r\n");
  ASSERT_TRUE(finished.wait(1000));
  EXPECT_TRUE(server.success());

}
