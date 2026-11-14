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

#include <QtGlobal>
#include <QCoreApplication>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QByteArray>
#include <QMap>
#include <QSignalSpy>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "constants/musixmatchsettings.h"
#include "lyrics/lyricssearchrequest.h"
#include "lyrics/musixmatchlyricsprovider.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kClientId[] = "secret-app-id";
constexpr char kClientSecret[] = "secret-signature-key";

// A reply which fails like a reply from Qt for an HTTP error, the error string contains the URL with the query.
class NotFoundNetworkReply : public QNetworkReply {
 public:
  explicit NotFoundNetworkReply(const QNetworkRequest &network_request, QObject *parent = nullptr) : QNetworkReply(parent) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
    setError(QNetworkReply::ContentNotFoundError, u"Error transferring %1 - server replied: Not Found"_s.arg(network_request.url().toDisplayString()));
    QMetaObject::invokeMethod(this, [this]() {
      setFinished(true);
      Q_EMIT finished();
    }, Qt::QueuedConnection);
  }

  void abort() override {}

 protected:
  qint64 readData(char *data, const qint64 maxlen) override {
    Q_UNUSED(data)
    Q_UNUSED(maxlen)
    return -1;
  }
};

// A successful reply with JSON.
class JsonNetworkReply : public QNetworkReply {
 public:
  explicit JsonNetworkReply(const QNetworkRequest &network_request, const QByteArray &data, QObject *parent = nullptr) : QNetworkReply(parent), data_(data) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    QMetaObject::invokeMethod(this, [this]() {
      setFinished(true);
      Q_EMIT finished();
    }, Qt::QueuedConnection);
  }

  void abort() override {}
  qint64 bytesAvailable() const override { return data_.size() + QNetworkReply::bytesAvailable(); }

 protected:
  qint64 readData(char *data, const qint64 maxlen) override {
    if (data_.isEmpty()) return -1;
    const qint64 len = qMin(maxlen, static_cast<qint64>(data_.size()));
    memcpy(data, data_.constData(), static_cast<size_t>(len));
    data_.remove(0, len);
    return len;
  }

 private:
  QByteArray data_;
};

// Replies with the JSON for the endpoints which have it, and with Not Found for the others.
class NotFoundNetworkAccessManager : public NetworkAccessManager {
 public:
  QList<QUrl> urls;
  QMap<QString, QByteArray> json_replies;

  QStringList Endpoints() const {
    QStringList endpoints;
    for (const QUrl &url : urls) {
      endpoints << url.path().section(u'/', -1);
    }
    return endpoints;
  }

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {
    Q_UNUSED(op)
    Q_UNUSED(outgoing_data)
    urls << network_request.url();
    const QString endpoint = network_request.url().path().section(u'/', -1);
    if (json_replies.contains(endpoint)) {
      return new JsonNetworkReply(network_request, json_replies.value(endpoint), this);
    }
    return new NotFoundNetworkReply(network_request, this);
  }
};

// Keeps the error messages, which are written to the log.
class TestMusixmatchLyricsProvider : public MusixmatchLyricsProvider {
 public:
  explicit TestMusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network) : MusixmatchLyricsProvider(network) {}

  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override {
    Q_UNUSED(debug_output)
    error_messages << error_message;
  }

  QStringList error_messages;
};

class MusixmatchLyricsProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // The settings are kept away from the users settings, and from other test processes.
    QStandardPaths::setTestModeEnabled(true);
    organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"StrawberryTest-"_s + QString::number(QCoreApplication::applicationPid()));

    network_ = make_shared<NotFoundNetworkAccessManager>();
    provider_ = std::make_unique<TestMusixmatchLyricsProvider>(network_);

    Settings s;
    s.remove(provider_->api_credentials_settings_group());
    s.beginGroup(provider_->api_credentials_settings_group());
    s.setValue(MusixmatchSettings::kUseCustomApiCredentials, true);
    s.setValue(MusixmatchSettings::kClientId, QLatin1String(kClientId));
    s.setValue(MusixmatchSettings::kClientSecret, QLatin1String(kClientSecret));
    s.endGroup();
    provider_->ReloadSettings();

  }

  void TearDown() override {
    if (provider_) {
      Settings s;
      s.remove(provider_->api_credentials_settings_group());
    }
    provider_.reset();
    QCoreApplication::setOrganizationName(organization_name_);
    QStandardPaths::setTestModeEnabled(false);
  }

  QString organization_name_;
  SharedPtr<NotFoundNetworkAccessManager> network_;
  std::unique_ptr<TestMusixmatchLyricsProvider> provider_;
};

// Regression test: the error string of a failed request contains the URL, so the app ID and the signature were written to the log.
TEST_F(MusixmatchLyricsProviderTest, FailedRequestErrorDoesNotContainCredentials) {

  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));

  ASSERT_TRUE(QTest::qWaitFor([this]() { return !provider_->error_messages.isEmpty(); }, 5000));
  ASSERT_EQ(1, network_->urls.count());

  // The request has the credentials, so the test fails if they are sent another way and the check below checks nothing.
  const QUrlQuery url_query(network_->urls.first());
  const QString signature = url_query.queryItemValue(u"signature"_s, QUrl::FullyDecoded);
  ASSERT_EQ(QLatin1String(kClientId), url_query.queryItemValue(u"app_id"_s));
  ASSERT_FALSE(signature.trimmed().isEmpty());

  const QString error_message = provider_->error_messages.first();
  EXPECT_TRUE(error_message.contains(u"Not Found"_s)) << error_message.toStdString();
  EXPECT_FALSE(error_message.contains(QLatin1String(kClientId))) << error_message.toStdString();
  EXPECT_FALSE(error_message.contains(signature.trimmed())) << error_message.toStdString();
  EXPECT_FALSE(error_message.contains(u"signature"_s)) << error_message.toStdString();

}

constexpr char kTokenReply[] = R"({"message":{"header":{"status_code":200},"body":{"user_token":"token"}}})";

// Regression test: the requested artist, album and title were used for the lyrics, but Musixmatch matches the track, so the lyrics always counted as a perfect match, also for a different track.
TEST_F(MusixmatchLyricsProviderTest, LyricsHaveMatchedTrack) {

  network_->json_replies.insert(u"token.get"_s, kTokenReply);
  network_->json_replies.insert(u"matcher.track.get"_s, R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"track_name":"Other title","artist_name":"Other artist","album_name":"Other album","has_lyrics":1}}}})");
  network_->json_replies.insert(u"track.lyrics.get"_s, R"({"message":{"header":{"status_code":200},"body":{"lyrics":{"lyrics_body":"Some lyrics"}}}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.album = u"Album"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_EQ(QStringList() << u"token.get"_s << u"matcher.track.get"_s << u"track.lyrics.get"_s, network_->Endpoints());
  EXPECT_EQ(u"1234"_s, QUrlQuery(network_->urls.last()).queryItemValue(u"track_id"_s));
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

  const LyricsSearchResults results = spy.first().at(1).value<LyricsSearchResults>();
  ASSERT_EQ(1, results.count());
  EXPECT_EQ(u"Other artist"_s, results.first().artist);
  EXPECT_EQ(u"Other album"_s, results.first().album);
  EXPECT_EQ(u"Other title"_s, results.first().title);
  EXPECT_EQ(u"Some lyrics"_s, results.first().lyrics);

}

// The lyrics are not requested for a matched track without lyrics.
TEST_F(MusixmatchLyricsProviderTest, MatchedTrackWithoutLyrics) {

  network_->json_replies.insert(u"token.get"_s, kTokenReply);
  network_->json_replies.insert(u"matcher.track.get"_s, R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"track_name":"Title","artist_name":"Artist","has_lyrics":0}}}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_EQ(QStringList() << u"token.get"_s << u"matcher.track.get"_s, network_->Endpoints());
  EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty());
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

}

}  // namespace
