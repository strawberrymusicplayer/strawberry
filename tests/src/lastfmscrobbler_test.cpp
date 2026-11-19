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

#include <cstring>
#include <memory>

#include "gtest_include.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <QMap>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/standardpaths.h"
#include "constants/lastfmsettings.h"
#include "constants/scrobblersettings.h"
#include "scrobbler/lastfmscrobbler.h"
#include "scrobbler/scrobblersettingsservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kCacheFile[] = "lastfmscrobbler.cache";
constexpr char kInvalidSessionKeyReply[] = R"({"error": 9, "message": "Invalid session key - Please re-authenticate"})";
constexpr char kServiceOfflineReply[] = R"({"error": 11, "message": "Service Offline"})";
constexpr char kNowPlayingReply[] = R"({"nowplaying": {}})";
constexpr char kInvalidParametersReply[] = R"({"error": 6, "message": "Invalid parameters"})";
constexpr char kInvalidApiKeyReply[] = R"({"error": 10, "message": "Invalid API key"})";
constexpr char kScrobbleAcceptedReply[] = R"({"scrobbles": {"@attr": {"accepted": 1, "ignored": 0}, "scrobble": {"artist": {"#text": "Artist"}, "album": {"#text": "Album"}, "albumArtist": {"#text": ""}, "track": {"#text": "Title"}, "timestamp": "1", "ignoredMessage": {"code": "0", "#text": ""}}}})";

// A reply which is finished by the test.
class ControlledNetworkReply : public QNetworkReply {
 public:
  explicit ControlledNetworkReply(const QNetworkRequest &network_request, QObject *parent = nullptr) : QNetworkReply(parent), pos_(0) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::PostOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
  }

  void abort() override {
    if (isFinished()) return;
    setError(QNetworkReply::OperationCanceledError, u"Operation canceled"_s);
    setFinished(true);
    Q_EMIT finished();
  }

  void Finish(const int http_status_code, const QByteArray &data) {
    if (isFinished()) return;
    data_ = data;
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, http_status_code);
    if (http_status_code == 401) {
      setError(QNetworkReply::AuthenticationRequiredError, u"Unauthorized"_s);
    }
    else if (http_status_code == 403) {
      setError(QNetworkReply::ContentAccessDenied, u"Forbidden"_s);
    }
    setFinished(true);
    Q_EMIT finished();
  }

  qint64 bytesAvailable() const override { return data_.size() - pos_ + QNetworkReply::bytesAvailable(); }

 protected:
  qint64 readData(char *data, const qint64 maxlen) override {
    const qint64 len = qMin(maxlen, data_.size() - pos_);
    if (len <= 0) return -1;
    memcpy(data, data_.constData() + pos_, static_cast<size_t>(len));
    pos_ += len;
    return len;
  }

 private:
  QByteArray data_;
  qint64 pos_;
};

// Keeps the replies open until the test finishes them.
class ControlledNetworkAccessManager : public NetworkAccessManager {
 public:
  QList<QPointer<ControlledNetworkReply>> replies;
  QList<QByteArray> request_bodies;

  // The titles of the tracks in the request, empty when it is not a request to scrobble.
  QStringList ScrobbledTitles(const qsizetype i) const {
    // The names of the parameters are percent encoded too.
    QMap<QString, QString> params;
    const QList<QByteArray> encoded_params = request_bodies.value(i).split('&');
    for (const QByteArray &encoded_param : encoded_params) {
      const qsizetype separator = encoded_param.indexOf('=');
      if (separator == -1) continue;
      params.insert(QUrl::fromPercentEncoding(encoded_param.left(separator)), QUrl::fromPercentEncoding(encoded_param.mid(separator + 1)));
    }
    if (params.value(u"method"_s) != "track.scrobble"_L1) return QStringList();
    QStringList titles;
    for (int track = 0; params.contains(u"track[%1]"_s.arg(track)); ++track) {
      titles << params.value(u"track[%1]"_s.arg(track));
    }
    return titles;
  }

  bool WaitForRequests(const qsizetype count, const int timeout_ms = 5000) const {
    return QTest::qWaitFor([this, count]() { return replies.count() >= count; }, timeout_ms) && replies.count() == count;
  }

  void FinishReply(const qsizetype i, const int http_status_code, const QByteArray &data) {
    if (replies.value(i)) replies.value(i)->Finish(http_status_code, data);
  }

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {
    Q_UNUSED(op)
    request_bodies << (outgoing_data ? outgoing_data->readAll() : QByteArray());
    ControlledNetworkReply *reply = new ControlledNetworkReply(network_request, this);
    replies << reply;
    return reply;
  }
};

class LastFMScrobblerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // The cache is kept out of the users cache, and away from other test processes, since the test removes it.
    QStandardPaths::setTestModeEnabled(true);
    organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"StrawberryTest-"_s + QString::number(QCoreApplication::applicationPid()));
    cache_path_ = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation);
    ASSERT_TRUE(QDir().mkpath(cache_path_));
    QFile::remove(cache_path_ + u'/' + QLatin1String(kCacheFile));

    Settings s;
    s.remove(QLatin1String(ScrobblerSettings::kSettingsGroup));
    s.beginGroup(ScrobblerSettings::kSettingsGroup);
    s.setValue(ScrobblerSettings::kEnabled, true);
    s.setValue(ScrobblerSettings::kOffline, false);
    s.setValue(ScrobblerSettings::kSubmit, 0);
    s.setValue(ScrobblerSettings::kShowErrorDialog, true);
    s.endGroup();

    s.remove(QLatin1String(LastFMScrobbler::kSettingsGroup));
    s.beginGroup(LastFMScrobbler::kSettingsGroup);
    s.setValue(ScrobblerSettings::kEnabled, true);
    s.setValue(LastFMSettings::kUseCustomApiCredentials, true);
    s.setValue(LastFMSettings::kClientId, u"api_key"_s);
    s.setValue(LastFMSettings::kClientSecret, u"api_secret"_s);
    s.setValue("username", u"user"_s);
    s.setValue("session_key", u"session_key"_s);
    s.endGroup();

    settings_ = make_shared<ScrobblerSettingsService>();
    network_ = make_shared<ControlledNetworkAccessManager>();
    scrobbler_ = std::make_unique<LastFMScrobbler>(settings_, network_);
    ASSERT_TRUE(scrobbler_->authenticated());

  }

  void TearDown() override {
    scrobbler_.reset();
    if (!cache_path_.isEmpty()) {
      QDir(cache_path_).removeRecursively();
    }
    QCoreApplication::setOrganizationName(organization_name_);
    QStandardPaths::setTestModeEnabled(false);
  }

  static Song MakeSong(const QString &title = u"Title"_s) {
    Song song(Song::Source::LocalFile);
    song.Init(title, u"Artist"_s, u"Album"_s, 180'000'000'000LL);
    song.set_url(QUrl(u"file:///music/%1.mp3"_s.arg(title)));
    return song;
  }

  // Plays and scrobbles the song, while a request to scrobble is waiting for its reply, or after an error, the scrobble is only added to the cache.
  void AddScrobble(const QString &title) {
    const Song song = MakeSong(title);
    const qsizetype requests = network_->replies.count();
    scrobbler_->UpdateNowPlaying(song);
    ASSERT_TRUE(network_->WaitForRequests(requests + 1));
    network_->FinishReply(requests, 200, QByteArray(kNowPlayingReply));
    scrobbler_->Scrobble(song);
    QCoreApplication::processEvents();
  }

  // Sends the next request to scrobble, and returns its titles.
  QStringList SendNextScrobbles() {
    const qsizetype requests = network_->replies.count();
    FireRetryTimer();
    if (!network_->WaitForRequests(requests + 1)) return QStringList();
    return network_->ScrobbledTitles(requests);
  }

  // Scrobbles the song, request 0 is now playing, request 1 is the scrobble.
  void ScrobbleSong() {
    const Song song = MakeSong();
    scrobbler_->UpdateNowPlaying(song);
    ASSERT_TRUE(network_->WaitForRequests(1));
    network_->FinishReply(0, 200, QByteArray(kNowPlayingReply));
    scrobbler_->Scrobble(song);
    ASSERT_TRUE(network_->WaitForRequests(2));
  }

  // Makes the retry timer time out now instead of after 30 seconds.
  void FireRetryTimer() {
    bool timer_active = false;
    const QList<QTimer*> timers = scrobbler_->findChildren<QTimer*>();
    for (QTimer *timer : timers) {
      if (!timer->isActive()) continue;
      timer_active = true;
      timer->start(0);
    }
    ASSERT_TRUE(timer_active);
    QTest::qWait(20);
  }

  static QString SessionKeyInSettings() {
    Settings s;
    s.beginGroup(LastFMScrobbler::kSettingsGroup);
    const QString session_key = s.value("session_key").toString();
    s.endGroup();
    return session_key;
  }

  QString organization_name_;
  QString cache_path_;
  SharedPtr<ScrobblerSettingsService> settings_;
  SharedPtr<ControlledNetworkAccessManager> network_;
  std::unique_ptr<LastFMScrobbler> scrobbler_;
};

// Regression test: a revoked session is reported with HTTP code 403, not 401, so the session was kept and the scrobble was sent again every 30 seconds, with an error dialog every time.
TEST_F(LastFMScrobblerTest, RevokedSessionStopsRetrying) {

  ScrobbleSong();
  if (HasFatalFailure()) return;
  QSignalSpy spy_error(&*scrobbler_, &ScrobblerService::ErrorMessage);

  network_->FinishReply(1, 403, QByteArray(kInvalidSessionKeyReply));
  QCoreApplication::processEvents();

  EXPECT_FALSE(scrobbler_->authenticated());
  EXPECT_TRUE(SessionKeyInSettings().isEmpty());
  EXPECT_EQ(1, spy_error.count());

  FireRetryTimer();
  if (HasFatalFailure()) return;

  EXPECT_EQ(2, network_->replies.count());
  EXPECT_EQ(1, spy_error.count());

}

// The scrobble is not lost, it's sent when the user has logged in again.
TEST_F(LastFMScrobblerTest, RevokedSessionKeepsScrobble) {

  ScrobbleSong();
  if (HasFatalFailure()) return;
  network_->FinishReply(1, 403, QByteArray(kInvalidSessionKeyReply));
  QCoreApplication::processEvents();
  ASSERT_FALSE(scrobbler_->authenticated());

  Settings s;
  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  s.setValue("username", u"user"_s);
  s.setValue("session_key", u"new_session_key"_s);
  s.endGroup();
  scrobbler_->LoadSession();
  FireRetryTimer();
  if (HasFatalFailure()) return;

  EXPECT_TRUE(network_->WaitForRequests(3));

}

// Other errors are temporary, the session is kept and the scrobble is sent again.
TEST_F(LastFMScrobblerTest, OtherErrorKeepsSession) {

  ScrobbleSong();
  if (HasFatalFailure()) return;

  network_->FinishReply(1, 503, QByteArray(kServiceOfflineReply));
  QCoreApplication::processEvents();

  EXPECT_TRUE(scrobbler_->authenticated());
  EXPECT_EQ(u"session_key"_s, SessionKeyInSettings());

}

// The session can have been replaced when the reply for the previous session arrives.
TEST_F(LastFMScrobblerTest, RejectedPreviousSessionKeepsNewSession) {

  scrobbler_->UpdateNowPlaying(MakeSong());
  ASSERT_TRUE(network_->WaitForRequests(1));

  Settings s;
  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  s.setValue("session_key", u"new_session_key"_s);
  s.endGroup();
  scrobbler_->LoadSession();

  network_->FinishReply(0, 403, QByteArray(kInvalidSessionKeyReply));
  QCoreApplication::processEvents();

  EXPECT_TRUE(scrobbler_->authenticated());
  EXPECT_EQ(u"new_session_key"_s, SessionKeyInSettings());

}

// A session rejected with HTTP code 401 is also cleared.
TEST_F(LastFMScrobblerTest, UnauthorizedClearsSession) {

  scrobbler_->UpdateNowPlaying(MakeSong());
  ASSERT_TRUE(network_->WaitForRequests(1));

  network_->FinishReply(0, 401, QByteArray());
  QCoreApplication::processEvents();

  EXPECT_FALSE(scrobbler_->authenticated());

}

// Regression test: when a request was rejected because of one of its scrobbles, the same scrobbles were sent again, and later scrobbles were never sent.
TEST_F(LastFMScrobblerTest, RejectedScrobbleDoesNotBlockLaterScrobbles) {

  // The first request fails, so the next scrobbles are sent together with it.
  AddScrobble(u"A"_s);
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"A"_s, network_->ScrobbledTitles(1));
  network_->FinishReply(1, 503, QByteArray(kServiceOfflineReply));
  QCoreApplication::processEvents();
  AddScrobble(u"B"_s);
  if (HasFatalFailure()) return;
  AddScrobble(u"C"_s);
  if (HasFatalFailure()) return;
  QSignalSpy spy_error(&*scrobbler_, &ScrobblerService::ErrorMessage);

  QStringList titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"A"_s << u"B"_s << u"C"_s, titles);
  network_->FinishReply(network_->replies.count() - 1, 400, QByteArray(kInvalidParametersReply));
  QCoreApplication::processEvents();

  // The scrobbles are sent one by one, the scrobble which is rejected is dropped.
  titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"A"_s, titles);
  network_->FinishReply(network_->replies.count() - 1, 400, QByteArray(kInvalidParametersReply));
  QCoreApplication::processEvents();
  ASSERT_FALSE(spy_error.isEmpty());
  EXPECT_TRUE(spy_error.last().at(0).toString().contains(u" - A "_s)) << spy_error.last().at(0).toString().toStdString();

  titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"B"_s, titles);
  network_->FinishReply(network_->replies.count() - 1, 200, QByteArray(kScrobbleAcceptedReply));
  QCoreApplication::processEvents();

  titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"C"_s, titles);
  network_->FinishReply(network_->replies.count() - 1, 200, QByteArray(kScrobbleAcceptedReply));
  QCoreApplication::processEvents();

  // A later scrobble is sent right away.
  AddScrobble(u"D"_s);
  if (HasFatalFailure()) return;
  ASSERT_TRUE(network_->WaitForRequests(network_->replies.count()));
  EXPECT_EQ(QStringList() << u"D"_s, network_->ScrobbledTitles(network_->replies.count() - 1));

}

// An error which is not caused by one scrobble keeps all the scrobbles, they are sent again together.
TEST_F(LastFMScrobblerTest, ErrorForAllScrobblesKeepsThem) {

  AddScrobble(u"A"_s);
  if (HasFatalFailure()) return;
  network_->FinishReply(1, 503, QByteArray(kServiceOfflineReply));
  QCoreApplication::processEvents();
  AddScrobble(u"B"_s);
  if (HasFatalFailure()) return;

  QStringList titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  ASSERT_EQ(QStringList() << u"A"_s << u"B"_s, titles);
  network_->FinishReply(network_->replies.count() - 1, 403, QByteArray(kInvalidApiKeyReply));
  QCoreApplication::processEvents();

  titles = SendNextScrobbles();
  if (HasFatalFailure()) return;
  EXPECT_EQ(QStringList() << u"A"_s << u"B"_s, titles);

}

}  // namespace
