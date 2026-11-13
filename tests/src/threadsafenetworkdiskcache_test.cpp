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

#include <memory>
#include <atomic>

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QHash>
#include <QIODevice>
#include <QPointer>
#include <QUrl>
#include <QVersionNumber>
#include <QDirIterator>
#include <QTemporaryDir>
#include <QThread>
#include <QMetaObject>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCacheMetaData>
#include <QAbstractNetworkCache>
#include <QStandardPaths>
#include <QTest>

#include "core/networkaccessmanager.h"
#include "core/threadsafenetworkdiskcache.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// Sets the directory the cache is created in, for the cache instances created while this exists.
// The cache is in the temporary directory on Windows, which the test mode of QStandardPaths doesn't change, so the tests would use and clear the real cache.
// On macOS, the cache is in the cache directory of QStandardPaths, which the test mode changes to a directory shared by all the tests, so the fixtures clear the cache.
class ScopedCacheDirectory {
 public:
  explicit ScopedCacheDirectory(const QString &path) {
#ifdef Q_OS_WIN32
    Set("TMP", path);
    Set("TEMP", path);
#else
    Set("XDG_CACHE_HOME", path);
#endif
  }

  ~ScopedCacheDirectory() {
    for (QHash<QByteArray, QByteArray>::const_iterator it = old_values_.constBegin(); it != old_values_.constEnd(); ++it) {
      if (it.value().isNull()) {
        qunsetenv(it.key().constData());
      }
      else {
        qputenv(it.key().constData(), it.value());
      }
    }
  }

  Q_DISABLE_COPY_MOVE(ScopedCacheDirectory)

 private:
  void Set(const QByteArray &name, const QString &path) {
    old_values_.insert(name, qEnvironmentVariableIsSet(name.constData()) ? qgetenv(name.constData()) : QByteArray());
    qputenv(name.constData(), path.toLocal8Bit());
  }

  QHash<QByteArray, QByteArray> old_values_;
};

// The cache is shared by all instances, like it's shared by all the network access managers.
// The tests call the cache like the network replies do: prepare() when the reply starts saving, insert() when it's done, and remove() to cancel, or right after prepare() when it didn't get a device.
class ThreadSafeNetworkDiskCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    QStandardPaths::setTestModeEnabled(true);
    ASSERT_TRUE(temp_dir_.isValid());
    cache_directory_ = std::make_unique<ScopedCacheDirectory>(temp_dir_.path());
    cache1_ = std::make_unique<ThreadSafeNetworkDiskCache>(nullptr);
    cache2_ = std::make_unique<ThreadSafeNetworkDiskCache>(nullptr);
    // On macOS, the test mode of QStandardPaths uses the same cache directory for all the tests, so it has the URLs saved by the previous tests.
    cache1_->clear();
  }

  void TearDown() override {
    cache1_.reset();
    cache2_.reset();
    cache_directory_.reset();
  }

  static QNetworkCacheMetaData MetaData(const bool save_to_disk = true, const QUrl &url = kUrl) {
    QNetworkCacheMetaData metadata;
    metadata.setUrl(url);
    metadata.setSaveToDisk(save_to_disk);
    // QNetworkDiskCache doesn't read cached data without headers.
    metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << QNetworkCacheMetaData::RawHeader("Content-Type", "image/jpeg"));
    return metadata;
  }

  QByteArray CachedData() const {
    std::unique_ptr<QIODevice> device(cache1_->data(kUrl));
    return device ? device->readAll() : QByteArray();
  }

  void Save(ThreadSafeNetworkDiskCache *cache, const QByteArray &data) {
    QIODevice *device = cache->prepare(MetaData());
    ASSERT_NE(nullptr, device);
    device->write(data);
    cache->insert(device);
  }

  inline static const QUrl kUrl = QUrl(u"https://example.com/cover.jpg"_s);

  QTemporaryDir temp_dir_;
  std::unique_ptr<ScopedCacheDirectory> cache_directory_;
  std::unique_ptr<ThreadSafeNetworkDiskCache> cache1_;
  std::unique_ptr<ThreadSafeNetworkDiskCache> cache2_;
};

TEST_F(ThreadSafeNetworkDiskCacheTest, SaveAndRead) {

  Save(&*cache1_, "cover");
  EXPECT_EQ("cover", CachedData());

#ifndef Q_OS_MACOS
  // The cache is in the temporary directory of the test, not in the real cache.
  QDirIterator it(temp_dir_.path(), QStringList() << u"*.d"_s, QDir::Files, QDirIterator::Subdirectories);
  EXPECT_TRUE(it.hasNext());
#endif

}

// Regression test: a reply which doesn't save the URL, for example because it's not to be stored, removed the URL, which also cancelled the saving of another reply, and deleted its device while it was still writing to it.
TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyNotSavingDoesNotCancelOtherSaving) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());
  device->write("cov");

  EXPECT_EQ(nullptr, cache2_->prepare(MetaData(false)));
  cache2_->remove(kUrl);

  ASSERT_FALSE(device.isNull());
  device->write("er");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

// Two replies saving the same URL at once, the reply cancelling would cancel the saving of the other reply, so only the first reply saves it.
TEST_F(ThreadSafeNetworkDiskCacheTest, OnlyOneReplySavesUrl) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  EXPECT_EQ(nullptr, cache2_->prepare(MetaData()));
  cache2_->remove(kUrl);

  ASSERT_FALSE(device.isNull());
  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

  // Saving it again works when the first reply is done.
  Save(&*cache2_, "new cover");
  EXPECT_EQ("new cover", CachedData());

}

// Replies in the same network access manager share the cache instance.
TEST_F(ThreadSafeNetworkDiskCacheTest, OnlyOneReplySavesUrlInSameInstance) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  EXPECT_EQ(nullptr, cache1_->prepare(MetaData()));
  cache1_->remove(kUrl);

  ASSERT_FALSE(device.isNull());
  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

// The reply saving the URL can still cancel it after another reply in the same network access manager was refused.
TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyCancelsItsSavingAfterOtherReplyWasRefused) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  EXPECT_EQ(nullptr, cache1_->prepare(MetaData()));
  cache1_->remove(kUrl);
  ASSERT_FALSE(device.isNull());

  cache1_->remove(kUrl);
  EXPECT_TRUE(device.isNull());

  Save(&*cache2_, "cover");
  EXPECT_EQ("cover", CachedData());

}

TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyCancelsItsSaving) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());
  device->write("cov");

  cache1_->remove(kUrl);
  EXPECT_TRUE(device.isNull());
  EXPECT_TRUE(CachedData().isEmpty());

  // Another reply can save it after it's cancelled.
  Save(&*cache2_, "cover");
  EXPECT_EQ("cover", CachedData());

}

// A reply which doesn't save the URL still removes it from the cache when no other reply is saving it, like with QNetworkDiskCache.
TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyNotSavingRemovesCachedUrl) {

  Save(&*cache1_, "cover");

  EXPECT_EQ(nullptr, cache2_->prepare(MetaData(false)));
  EXPECT_TRUE(cache2_->remove(kUrl));
  EXPECT_TRUE(CachedData().isEmpty());

}

// Regression test: QNetworkCacheMetaData removes the password and the fragment from the URL, and the reply cancels the saving with its URL, which has them, so the saving wasn't cancelled.
TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyCancelsItsSavingOfUrlWithFragment) {

  const QUrl url(u"https://user:password@example.com/cover.jpg#art"_s);
  QPointer<QIODevice> device = cache1_->prepare(MetaData(true, url));
  ASSERT_FALSE(device.isNull());

  cache1_->remove(url);
  EXPECT_TRUE(device.isNull());

  Save(&*cache2_, "cover");
  EXPECT_EQ("cover", CachedData());

}

// A reply which doesn't get a device doesn't cancel the other reply's saving, also when the URLs have different fragments.
TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyNotSavingUrlWithFragmentDoesNotCancelOtherSaving) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData(true, QUrl(u"https://example.com/cover.jpg#front"_s)));
  ASSERT_FALSE(device.isNull());

  const QUrl url(u"https://example.com/cover.jpg#back"_s);
  EXPECT_EQ(nullptr, cache2_->prepare(MetaData(true, url)));
  cache2_->remove(url);

  ASSERT_FALSE(device.isNull());
  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

TEST_F(ThreadSafeNetworkDiskCacheTest, ReplyNotSavingUrlWithFragmentDoesNotCancelOtherSavingInSameInstance) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData(true, QUrl(u"https://example.com/cover.jpg#front"_s)));
  ASSERT_FALSE(device.isNull());

  const QUrl url(u"https://example.com/cover.jpg#back"_s);
  EXPECT_EQ(nullptr, cache1_->prepare(MetaData(true, url)));
  cache1_->remove(url);

  ASSERT_FALSE(device.isNull());
  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

// Regression test: before Qt 6.5.10, 6.8.5 and 6.9.2, a reply deleted while saving the URL didn't cancel the saving, so the URL was never saved again.
TEST_F(ThreadSafeNetworkDiskCacheTest, DeletedReplyCancelsItsSaving) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());
  device->write("cov");

  cache1_->CancelUnusedSavings(QList<QUrl>());
  EXPECT_TRUE(device.isNull());
  EXPECT_TRUE(CachedData().isEmpty());

  Save(&*cache2_, "cover");
  EXPECT_EQ("cover", CachedData());

}

// Another reply of the same network access manager with the URL can be the reply saving it.
TEST_F(ThreadSafeNetworkDiskCacheTest, SavingIsKeptWhileOtherReplyHasUrl) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  cache1_->CancelUnusedSavings(QList<QUrl>() << QUrl(u"https://example.com/other.jpg"_s) << kUrl);
  ASSERT_FALSE(device.isNull());
  EXPECT_EQ(nullptr, cache2_->prepare(MetaData()));
  cache2_->remove(kUrl);

  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

// The replies of another network access manager don't save the URLs of this one.
TEST_F(ThreadSafeNetworkDiskCacheTest, DeletedReplyDoesNotCancelSavingOfOtherCache) {

  QPointer<QIODevice> device = cache1_->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  cache2_->CancelUnusedSavings(QList<QUrl>());
  ASSERT_FALSE(device.isNull());

  device->write("cover");
  cache1_->insert(device);
  EXPECT_EQ("cover", CachedData());

}

// From Qt 6.5.10, 6.8.5 and 6.9.2, the deleted reply cancels its saving itself, after the saving was cancelled when it was deleted, and another reply can have started saving the URL already.
TEST_F(ThreadSafeNetworkDiskCacheTest, DeletedReplyCancellingDoesNotCancelOtherSaving) {

  QPointer<QIODevice> device1 = cache1_->prepare(MetaData());
  ASSERT_FALSE(device1.isNull());
  cache1_->CancelUnusedSavings(QList<QUrl>());

  QPointer<QIODevice> device2 = cache2_->prepare(MetaData());
  ASSERT_FALSE(device2.isNull());
  device2->write("cov");

  EXPECT_FALSE(cache1_->remove(kUrl));

  ASSERT_FALSE(device2.isNull());
  device2->write("er");
  cache2_->insert(device2);
  EXPECT_EQ("cover", CachedData());

}

// The replies of a network access manager are deleted before its cache, without a reply deleted signal from the network access manager.
TEST_F(ThreadSafeNetworkDiskCacheTest, DeletedCacheCancelsItsSavings) {

  std::unique_ptr<ThreadSafeNetworkDiskCache> cache3 = std::make_unique<ThreadSafeNetworkDiskCache>(nullptr);
  QPointer<QIODevice> device = cache3->prepare(MetaData());
  ASSERT_FALSE(device.isNull());

  cache3.reset();
  EXPECT_TRUE(device.isNull());

  Save(&*cache1_, "cover");
  EXPECT_EQ("cover", CachedData());

}

// Replies to all requests with a cacheable body, sending the first half at once, and the rest when Release() is called, unless Hold(false) was called.
class FakeHttpServer : public QTcpServer {
 public:
  explicit FakeHttpServer(QObject *parent = nullptr) : QTcpServer(parent), body_(Body()), hold_(true) {
    QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
      while (QTcpSocket *socket = nextPendingConnection()) {
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() { ReadyRead(socket); });
      }
    });
  }

  // Larger than the download buffer QNetworkAccessManager uses for small replies, which are only saved in the cache when they are complete.
  static QByteArray Body() { return QByteArray(256 * 1024, 'x') + QByteArray(256 * 1024, 'y'); }

  QUrl Url() const { return QUrl(u"http://127.0.0.1:%1/lyrics.txt"_s.arg(serverPort())); }
  qsizetype held_count() const { return held_.count(); }
  void Hold(const bool hold) { hold_ = hold; }
  void SetBody(const QByteArray &body) { body_ = body; }
  void SetContentEncoding(const QByteArray &content_encoding) { content_encoding_ = content_encoding; }

  void Release() {
    const QList<QPointer<QTcpSocket>> held = held_;
    held_.clear();
    for (const QPointer<QTcpSocket> &socket : held) {
      if (socket && socket->state() == QAbstractSocket::ConnectedState) socket->write(body_.mid(body_.size() / 2));
    }
  }

 private:
  void ReadyRead(QTcpSocket *socket) {
    QByteArray &request = requests_[socket];
    request.append(socket->readAll());
    if (!request.contains("\r\n\r\n")) return;
    request.clear();
    const QByteArray body = body_;
    // Text is saved in a buffer in the cache, which isn't closed when it's deleted, so the reply isn't told.
    const QByteArray content_encoding_header = content_encoding_.isEmpty() ? QByteArray() : "Content-Encoding: " + content_encoding_ + "\r\n";
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n" + content_encoding_header + "Cache-Control: max-age=3600\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n");
    if (hold_) {
      socket->write(body.left(body.size() / 2));
      held_ << socket;
    }
    else {
      socket->write(body);
    }
  }

  QByteArray body_;
  QByteArray content_encoding_;
  bool hold_;
  QList<QPointer<QTcpSocket>> held_;
  QHash<QTcpSocket*, QByteArray> requests_;
};

}  // namespace

// Network access managers using the cache, with replies for the same URL at once, while one of them is saving the URL in the cache.
// It's not in the anonymous namespace, since it's a friend of NetworkAccessManager.
class ThreadSafeNetworkDiskCacheNetworkTest : public ::testing::Test {
 protected:
  enum class Cancel {
    Abort,
    Delete,
  };

  void SetUp() override {
    QStandardPaths::setTestModeEnabled(true);
    ASSERT_TRUE(temp_dir_.isValid());
    cache_directory_ = std::make_unique<ScopedCacheDirectory>(temp_dir_.path());
    ASSERT_TRUE(server_.listen(QHostAddress::LocalHost));
    network1_ = std::make_unique<NetworkAccessManager>();
    // On macOS, the test mode of QStandardPaths uses the same cache directory for all the tests.
    network1_->cache()->clear();
  }

  void TearDown() override {
    if (thread_.isRunning()) {
      // Posted directly, since a queued call isn't run when the event loop of the thread has stopped already.
      network2_->deleteLater();
      thread_.quit();
      thread_.wait();
    }
    network1_.reset();
    cache_directory_.reset();
  }

  // Starts the second reply in a network access manager in another thread, and returns when it has finished.
  void StartInThread() {
    thread_.start();
    network2_ = new NetworkAccessManager;
    network2_->moveToThread(&thread_);
    QMetaObject::invokeMethod(network2_, [this]() {
      QNetworkReply *reply = network2_->get(QNetworkRequest(server_.Url()));
      QObject::connect(reply, &QNetworkReply::finished, reply, [this, reply]() {
        thread_error_ = reply->error();
        thread_data_ = reply->readAll();
        reply->deleteLater();
        thread_finished_ = true;
      });
    }, Qt::BlockingQueuedConnection);
  }

  // Saves the URL for one of the replies, which don't start saving it, since the server doesn't reply.
  static QPointer<QIODevice> PrepareSaving(QNetworkAccessManager *network, const QUrl &url) {
    QNetworkCacheMetaData metadata;
    metadata.setUrl(url);
    metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << QNetworkCacheMetaData::RawHeader("Content-Type", "text/plain"));
    return network->cache()->prepare(metadata);
  }

  static bool QtCancelsSavingOfDeletedReply(const QVersionNumber &qt_version) {
    return NetworkAccessManager::QtCancelsSavingOfDeletedReply(qt_version);
  }

  // Gets the URL of the server with a reply which doesn't save it in the cache, so the test can save it for the reply, and returns when the reply has finished, or got the headers when the server holds the rest.
  QNetworkReply *GetWithoutSaving() {
    QNetworkRequest network_request(server_.Url());
    network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    network_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    QNetworkReply *reply = network1_->get(network_request);
    EXPECT_TRUE(QTest::qWaitFor([reply]() { return reply->isFinished() || reply->hasRawHeader("Content-Type"); }, 5000));
    return reply;
  }

  void SetCancelSavingsOfFinishedReplies(const bool cancel) {
    network1_->cancel_savings_of_finished_replies_ = cancel;
  }

  QByteArray CachedData() const {
    std::unique_ptr<QIODevice> device(network1_->cache()->data(server_.Url()));
    return device ? device->readAll() : QByteArray();
  }

  // Cancels reply1 while reply2 keeps running, with reply2 started before or after reply1, since QNetworkDiskCache cancelled the first saving of the URL it found, which depends on the order.
  void Run(const bool in_thread, const Cancel cancel, const bool reply2_first) {

    QPointer<QNetworkReply> reply1;
    QPointer<QNetworkReply> reply2;
    if (!reply2_first) {
      reply1 = network1_->get(QNetworkRequest(server_.Url()));
      ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.held_count() == 1; }, 5000));
    }
    if (in_thread) {
      StartInThread();
    }
    else {
      reply2 = network1_->get(QNetworkRequest(server_.Url()));
    }
    if (reply2_first) {
      ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.held_count() == 1; }, 5000));
      reply1 = network1_->get(QNetworkRequest(server_.Url()));
    }

    // Both replies are saving or refused, and the first half of the body is being written.
    ASSERT_TRUE(QTest::qWaitFor([this]() { return server_.held_count() == 2; }, 5000));
    ASSERT_TRUE(QTest::qWaitFor([&reply1]() { return reply1->bytesAvailable() >= FakeHttpServer::Body().size() / 2; }, 5000));
    QTest::qWait(100);

    if (cancel == Cancel::Abort) {
      reply1->abort();
      reply1->deleteLater();
    }
    else {
      delete reply1;
    }

    server_.Release();

    if (in_thread) {
      ASSERT_TRUE(QTest::qWaitFor([this]() { return thread_finished_.load(); }, 5000));
      EXPECT_EQ(QNetworkReply::NoError, thread_error_);
      EXPECT_EQ(FakeHttpServer::Body(), thread_data_);
    }
    else {
      ASSERT_TRUE(QTest::qWaitFor([&reply2]() { return reply2->isFinished(); }, 5000));
      EXPECT_EQ(QNetworkReply::NoError, reply2->error()) << reply2->errorString().toStdString();
      EXPECT_EQ(FakeHttpServer::Body(), reply2->readAll());
      reply2->deleteLater();
    }

    // The URL is either cached by the reply which finished, or not when the cancelled reply was saving it, never partly.
    const QByteArray cached_data = CachedData();
    EXPECT_TRUE(cached_data.isEmpty() || cached_data == FakeHttpServer::Body()) << cached_data.size();

    // The URL is saved again by the next reply, which gets another body, so the cached data shows that it saved it.
    const QByteArray new_body(512 * 1024, 'z');
    server_.Hold(false);
    server_.SetBody(new_body);
    QNetworkRequest network_request(server_.Url());
    network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply3 = network1_->get(network_request);
    ASSERT_TRUE(QTest::qWaitFor([reply3]() { return reply3->isFinished(); }, 5000));
    EXPECT_EQ(QNetworkReply::NoError, reply3->error()) << reply3->errorString().toStdString();
    delete reply3;
    EXPECT_EQ(new_body, CachedData());

  }

  QTemporaryDir temp_dir_;
  std::unique_ptr<ScopedCacheDirectory> cache_directory_;
  FakeHttpServer server_;
  std::unique_ptr<NetworkAccessManager> network1_;
  QThread thread_;
  NetworkAccessManager *network2_ = nullptr;
  std::atomic<bool> thread_finished_ = false;
  QNetworkReply::NetworkError thread_error_ = QNetworkReply::NoError;
  QByteArray thread_data_;
};

// The network access manager tells the cache when a reply is deleted, so the saving of a reply deleted while saving is cancelled when none of the other replies has the URL.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, DeletedReplyCancelsSavingWhenNoOtherReplyHasUrl) {

  // The server doesn't reply, so the replies don't start saving, the test saves for them.
  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  const QUrl url(u"http://127.0.0.1:%1/lyrics.txt"_s.arg(silent_server.serverPort()));
  QNetworkReply *reply1 = network1_->get(QNetworkRequest(url));
  QNetworkReply *reply2 = network1_->get(QNetworkRequest(url));
  QNetworkReply *reply3 = network1_->get(QNetworkRequest(server_.Url()));

  QNetworkCacheMetaData metadata;
  metadata.setUrl(url);
  metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << QNetworkCacheMetaData::RawHeader("Content-Type", "text/plain"));
  QPointer<QIODevice> device = network1_->cache()->prepare(metadata);
  ASSERT_FALSE(device.isNull());

  delete reply1;
  EXPECT_FALSE(device.isNull());
  delete reply3;
  EXPECT_FALSE(device.isNull());
  delete reply2;
  EXPECT_TRUE(device.isNull());

  // Another network access manager can save the URL.
  std::unique_ptr<NetworkAccessManager> network = std::make_unique<NetworkAccessManager>();
  QIODevice *device2 = network->cache()->prepare(metadata);
  ASSERT_NE(nullptr, device2);
  device2->write("lyrics");
  network->cache()->insert(device2);
  std::unique_ptr<QIODevice> cached_device(network1_->cache()->data(url));
  ASSERT_NE(nullptr, cached_device);
  EXPECT_EQ("lyrics", cached_device->readAll());

}

// Regression test: the saving of a reply with a password or a fragment in its URL was cancelled when another reply was deleted, while the reply was still using it, since the saving has the URL without them.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, ReplyWithFragmentKeepsSavingWhenOtherReplyIsDeleted) {

  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  const QUrl url(u"http://user:password@127.0.0.1:%1/cover.jpg#art"_s.arg(silent_server.serverPort()));
  std::unique_ptr<QNetworkReply> reply1(network1_->get(QNetworkRequest(url)));
  QNetworkReply *reply2 = network1_->get(QNetworkRequest(QUrl(u"http://127.0.0.1:%1/other.jpg"_s.arg(silent_server.serverPort()))));
  QPointer<QIODevice> device = PrepareSaving(&*network1_, url);
  ASSERT_FALSE(device.isNull());

  delete reply2;
  EXPECT_FALSE(device.isNull());

  reply1.reset();
  EXPECT_TRUE(device.isNull());

}

// Before Qt 6.5.10, 6.8.5 and 6.9.2, a reply which failed before it got all the data never cancels its saving, so it's cancelled when it has finished, also while the reply isn't deleted.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, FailedReplyCancelsSavingWithOlderQt) {

  SetCancelSavingsOfFinishedReplies(true);
  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  const QUrl url(u"http://127.0.0.1:%1/lyrics.txt"_s.arg(silent_server.serverPort()));
  std::unique_ptr<QNetworkReply> reply1(network1_->get(QNetworkRequest(url)));
  std::unique_ptr<QNetworkReply> reply2(network1_->get(QNetworkRequest(url)));
  QPointer<QIODevice> device = PrepareSaving(&*network1_, url);
  ASSERT_FALSE(device.isNull());

  // The other reply with the URL can be the reply saving it.
  reply1->abort();
  EXPECT_FALSE(device.isNull());

  reply2->abort();
  EXPECT_TRUE(device.isNull());

}

// Regression test: a failed reply which wasn't deleted yet kept the saving of a deleted reply with the same URL, so the URL wasn't saved again until the failed reply was deleted.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, FailedReplyDoesNotKeepSavingOfDeletedReplyWithOlderQt) {

  SetCancelSavingsOfFinishedReplies(true);
  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  const QUrl url(u"http://127.0.0.1:%1/lyrics.txt"_s.arg(silent_server.serverPort()));
  std::unique_ptr<QNetworkReply> reply1(network1_->get(QNetworkRequest(url)));
  QNetworkReply *reply2 = network1_->get(QNetworkRequest(url));
  reply1->abort();
  QPointer<QIODevice> device = PrepareSaving(&*network1_, url);
  ASSERT_FALSE(device.isNull());

  delete reply2;
  EXPECT_TRUE(device.isNull());

}

// A reply which has finished without an error can still use its saving, when the data is compressed, it's written to the saving when it's read.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, SucceededReplyKeepsSavingWithOlderQt) {

  SetCancelSavingsOfFinishedReplies(true);
  server_.Hold(false);
  std::unique_ptr<QNetworkReply> reply1(GetWithoutSaving());
  ASSERT_TRUE(QTest::qWaitFor([&reply1]() { return reply1->isFinished(); }, 5000));
  ASSERT_EQ(QNetworkReply::NoError, reply1->error());
  QPointer<QIODevice> device = PrepareSaving(&*network1_, server_.Url());
  ASSERT_FALSE(device.isNull());

  // Another reply failing cancels the unused savings.
  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  std::unique_ptr<QNetworkReply> reply2(network1_->get(QNetworkRequest(QUrl(u"http://127.0.0.1:%1/other.txt"_s.arg(silent_server.serverPort())))));
  reply2->abort();
  EXPECT_FALSE(device.isNull());

}

// A reply with compressed data which has failed can still use its saving, the data received is written to the saving when it's read.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, FailedReplyWithCompressedDataKeepsSavingWithOlderQt) {

  SetCancelSavingsOfFinishedReplies(true);
  server_.SetContentEncoding("gzip");
  std::unique_ptr<QNetworkReply> reply1(GetWithoutSaving());
  ASSERT_TRUE(reply1->hasRawHeader("Content-Encoding"));
  QPointer<QIODevice> device = PrepareSaving(&*network1_, server_.Url());
  ASSERT_FALSE(device.isNull());

  reply1->abort();
  EXPECT_FALSE(device.isNull());

  reply1.reset();
  EXPECT_TRUE(device.isNull());

}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, QtVersionsCancellingSavingOfDeletedReply) {

  EXPECT_FALSE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 4, 0)));
  EXPECT_FALSE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 5, 9)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 5, 10)));
  EXPECT_FALSE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 6, 3)));
  EXPECT_FALSE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 8, 4)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 8, 5)));
  EXPECT_FALSE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 9, 1)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 9, 2)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 10, 0)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(6, 13, 0)));
  EXPECT_TRUE(QtCancelsSavingOfDeletedReply(QVersionNumber(7, 0, 0)));

}

// With the later Qt versions, a reply cancels its saving itself when it's deleted, so the saving is kept until then, cancelling it before could cancel another reply's saving of the URL.
TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, FailedReplyKeepsSavingUntilDeleted) {

  SetCancelSavingsOfFinishedReplies(false);
  QTcpServer silent_server;
  ASSERT_TRUE(silent_server.listen(QHostAddress::LocalHost));
  const QUrl url(u"http://127.0.0.1:%1/lyrics.txt"_s.arg(silent_server.serverPort()));
  std::unique_ptr<QNetworkReply> reply1(network1_->get(QNetworkRequest(url)));
  QPointer<QIODevice> device = PrepareSaving(&*network1_, url);
  ASSERT_FALSE(device.isNull());

  reply1->abort();
  EXPECT_FALSE(device.isNull());

  reply1.reset();
  EXPECT_TRUE(device.isNull());

}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, AbortFirstReplyInSameManager) {
  Run(false, Cancel::Abort, false);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, AbortSecondReplyInSameManager) {
  Run(false, Cancel::Abort, true);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, DeleteFirstReplyInSameManager) {
  Run(false, Cancel::Delete, false);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, DeleteSecondReplyInSameManager) {
  Run(false, Cancel::Delete, true);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, AbortFirstReplyWithManagerInOtherThread) {
  Run(true, Cancel::Abort, false);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, AbortSecondReplyWithManagerInOtherThread) {
  Run(true, Cancel::Abort, true);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, DeleteFirstReplyWithManagerInOtherThread) {
  Run(true, Cancel::Delete, false);
}

TEST_F(ThreadSafeNetworkDiskCacheNetworkTest, DeleteSecondReplyWithManagerInOtherThread) {
  Run(true, Cancel::Delete, true);
}
