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
#include <QList>
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

constexpr char kApiKey[] = "secret-api-key";

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

// A reply which fails with an HTTP error and has data, it counts how many bytes of the data are read.
class HttpErrorNetworkReply : public QNetworkReply {
 public:
  explicit HttpErrorNetworkReply(const QNetworkRequest &network_request, const QByteArray &data, qint64 *bytes_read, QObject *parent = nullptr) : QNetworkReply(parent), data_(data), bytes_read_(bytes_read) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 500);
    setError(QNetworkReply::InternalServerError, u"Error transferring %1 - server replied: Internal Server Error"_s.arg(network_request.url().toDisplayString()));
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
    *bytes_read_ += len;
    return len;
  }

 private:
  QByteArray data_;
  qint64 *bytes_read_;
};

// Replies with the JSON for the endpoints which have it, with an HTTP error with data for the endpoints which have it, and with Not Found for the others.
class NotFoundNetworkAccessManager : public NetworkAccessManager {
 public:
  QList<QUrl> urls;
  QMap<QString, QByteArray> json_replies;
  QMap<QString, QByteArray> http_error_replies;
  qint64 http_error_bytes_read = 0;

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
    if (http_error_replies.contains(endpoint)) {
      return new HttpErrorNetworkReply(network_request, http_error_replies.value(endpoint), &http_error_bytes_read, this);
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
    s.setValue(MusixmatchSettings::kApiKey, QLatin1String(kApiKey));
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

// Regression test: the error string of a failed request contains the URL, so the API key was written to the log.
TEST_F(MusixmatchLyricsProviderTest, FailedRequestErrorDoesNotContainApiKey) {

  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));

  ASSERT_TRUE(QTest::qWaitFor([this]() { return !provider_->error_messages.isEmpty(); }, 5000));
  ASSERT_EQ(1, network_->urls.count());

  // The request has the API key, so the test fails if it's sent another way and the check below checks nothing.
  ASSERT_EQ(QLatin1String(kApiKey), QUrlQuery(network_->urls.first()).queryItemValue(u"apikey"_s));

  const QString error_message = provider_->error_messages.first();
  EXPECT_TRUE(error_message.contains(u"Not Found"_s)) << error_message.toStdString();
  EXPECT_FALSE(error_message.contains(QLatin1String(kApiKey))) << error_message.toStdString();

}

// Without an API key, nothing is requested.
TEST_F(MusixmatchLyricsProviderTest, MissingApiKey) {

  Settings s;
  s.beginGroup(provider_->api_credentials_settings_group());
  s.setValue(MusixmatchSettings::kApiKey, QString());
  s.endGroup();
  provider_->ReloadSettings();

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(network_->urls.isEmpty());
  EXPECT_EQ(1, provider_->error_messages.count());
  EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty());

}

constexpr char kMatchedTrackReply[] = R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"commontrack_id":5678,"track_name":"Other title","artist_name":"Other artist","album_name":"Other album","instrumental":0,"has_lyrics":1,"restricted":0}}}})";

// Regression test: the requested artist, album and title were used for the lyrics, but Musixmatch matches the track, so the lyrics always counted as a perfect match, also for a different track.
TEST_F(MusixmatchLyricsProviderTest, LyricsHaveMatchedTrack) {

  network_->json_replies.insert(u"matcher.track.get"_s, kMatchedTrackReply);
  network_->json_replies.insert(u"track.lyrics.get"_s, R"({"message":{"header":{"status_code":200},"body":{"lyrics":{"lyrics_id":1,"restricted":0,"lyrics_body":"Some lyrics\n","lyrics_copyright":"Lyrics powered by www.musixmatch.com"}}}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.album = u"Album"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_EQ(QStringList() << u"matcher.track.get"_s << u"track.lyrics.get"_s, network_->Endpoints());
  const QUrlQuery track_query(network_->urls.first());
  EXPECT_EQ(u"Artist"_s, track_query.queryItemValue(u"q_artist"_s));
  EXPECT_EQ(u"Title"_s, track_query.queryItemValue(u"q_track"_s));
  // The lyrics are requested with the common track ID, track.lyrics.get doesn't take the track ID.
  const QUrlQuery lyrics_query(network_->urls.last());
  EXPECT_EQ(u"5678"_s, lyrics_query.queryItemValue(u"commontrack_id"_s));
  EXPECT_EQ(QLatin1String(kApiKey), lyrics_query.queryItemValue(u"apikey"_s));
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

  const LyricsSearchResults results = spy.first().at(1).value<LyricsSearchResults>();
  ASSERT_EQ(1, results.count());
  EXPECT_EQ(u"Other artist"_s, results.first().artist);
  EXPECT_EQ(u"Other album"_s, results.first().album);
  EXPECT_EQ(u"Other title"_s, results.first().title);
  // The copyright must be shown with the lyrics.
  EXPECT_EQ(u"Some lyrics\n\nLyrics powered by www.musixmatch.com"_s, results.first().lyrics);

}

// The lyrics are not requested for a matched track without lyrics, an instrumental track, or a track the lyrics of which may not be shown.
TEST_F(MusixmatchLyricsProviderTest, MatchedTrackWithoutLyricsToShow) {

  const QList<QByteArray> track_replies = QList<QByteArray>()
    << R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"commontrack_id":5678,"track_name":"Title","artist_name":"Artist","has_lyrics":0}}}})"
    << R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"commontrack_id":5678,"track_name":"Title","artist_name":"Artist","has_lyrics":1,"instrumental":1}}}})"
    << R"({"message":{"header":{"status_code":200},"body":{"track":{"track_id":1234,"commontrack_id":5678,"track_name":"Title","artist_name":"Artist","has_lyrics":1,"restricted":1}}}})";

  int id = 0;
  for (const QByteArray &track_reply : track_replies) {
    network_->urls.clear();
    network_->json_replies.insert(u"matcher.track.get"_s, track_reply);

    QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
    LyricsSearchRequest request;
    request.artist = u"Artist"_s;
    request.title = u"Title"_s;
    ASSERT_TRUE(provider_->StartSearchAsync(++id, request));
    ASSERT_TRUE(spy.wait(5000));

    EXPECT_EQ(QStringList() << u"matcher.track.get"_s, network_->Endpoints()) << track_reply.toStdString();
    EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty()) << track_reply.toStdString();
  }
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

}

// Lyrics which may not be shown in this country have an empty body and a message instead of the copyright, they are not used.
TEST_F(MusixmatchLyricsProviderTest, RestrictedLyrics) {

  network_->json_replies.insert(u"matcher.track.get"_s, kMatchedTrackReply);
  network_->json_replies.insert(u"track.lyrics.get"_s, R"({"message":{"header":{"status_code":200},"body":{"lyrics":{"lyrics_id":1,"restricted":1,"lyrics_body":"","lyrics_copyright":"Unfortunately we're not authorized to show these lyrics."}}}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty());
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

}

// The API replies with HTTP status 200, and the status in the header of the message.
TEST_F(MusixmatchLyricsProviderTest, RejectedApiKeyIsReported) {

  network_->json_replies.insert(u"matcher.track.get"_s, R"({"message":{"header":{"status_code":401},"body":[]}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_EQ(QStringList() << u"matcher.track.get"_s, network_->Endpoints());
  ASSERT_EQ(1, provider_->error_messages.count());
  EXPECT_TRUE(provider_->error_messages.first().contains(u"API key"_s)) << provider_->error_messages.first().toStdString();
  EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty());

}

// No match is not an error.
TEST_F(MusixmatchLyricsProviderTest, NoMatch) {

  network_->json_replies.insert(u"matcher.track.get"_s, R"({"message":{"header":{"status_code":404},"body":[]}})");

  QSignalSpy spy(&*provider_, &LyricsProvider::SearchFinished);
  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(spy.first().at(1).value<LyricsSearchResults>().isEmpty());
  EXPECT_TRUE(provider_->error_messages.isEmpty()) << provider_->error_messages.join(u", "_s).toStdString();

}

// Regression test: the data of a reply with an HTTP error was not read, which is needed for QTBUG-135641.
TEST_F(MusixmatchLyricsProviderTest, HttpErrorReplyDataIsRead) {

  constexpr char kErrorData[] = R"({"message":{"header":{"status_code":500}}})";
  network_->http_error_replies.insert(u"matcher.track.get"_s, kErrorData);

  LyricsSearchRequest request;
  request.artist = u"Artist"_s;
  request.title = u"Title"_s;
  ASSERT_TRUE(provider_->StartSearchAsync(1, request));

  ASSERT_TRUE(QTest::qWaitFor([this]() { return !provider_->error_messages.isEmpty(); }, 5000));
  EXPECT_EQ(static_cast<qint64>(sizeof(kErrorData) - 1), network_->http_error_bytes_read);
  EXPECT_FALSE(provider_->error_messages.first().contains(QLatin1String(kApiKey))) << provider_->error_messages.first().toStdString();

}

}  // namespace
