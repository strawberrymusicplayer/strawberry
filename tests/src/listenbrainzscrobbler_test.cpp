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
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>
#include <QString>
#include <QTest>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/standardpaths.h"
#include "constants/listenbrainzsettings.h"
#include "constants/scrobblersettings.h"
#include "scrobbler/listenbrainzscrobbler.h"
#include "scrobbler/scrobblercache.h"
#include "scrobbler/scrobblersettingsservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kCacheFile[] = "listenbrainzscrobbler.cache";

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
  QList<QUrl> urls;

  bool WaitForRequests(const qsizetype count, const int timeout_ms = 5000) const {
    return QTest::qWaitFor([this, count]() { return replies.count() >= count; }, timeout_ms) && replies.count() == count;
  }

  void FinishReply(const qsizetype i, const int http_status_code, const QByteArray &data) {
    if (replies.value(i)) replies.value(i)->Finish(http_status_code, data);
  }

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {
    Q_UNUSED(op)
    Q_UNUSED(outgoing_data)
    ControlledNetworkReply *reply = new ControlledNetworkReply(network_request, this);
    replies << reply;
    urls << network_request.url();
    return reply;
  }
};

class ListenBrainzScrobblerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // The cache is kept out of the users cache, and away from other test processes, since the test removes and writes it.
    // On Unix, except macOS, the cache location is in XDG_CACHE_HOME, which is set to a directory of the process by main.cpp.
    // On Windows and macOS, StandardPaths uses QStandardPaths, where the test mode gives a test location shared by every process, so the organization name, which is part of the cache location, is made specific to the process.
    // The application name is kept, it's part of the user agent.
    QStandardPaths::setTestModeEnabled(true);
    organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"StrawberryTest-"_s + QString::number(QCoreApplication::applicationPid()));
    cache_path_ = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation);
    // The cache directory is not created by anything else in the test.
    ASSERT_TRUE(QDir().mkpath(cache_path_));
    QFile::remove(cache_path_ + u'/' + QLatin1String(kCacheFile));

    Settings s;
    s.remove(QLatin1String(ScrobblerSettings::kSettingsGroup));
    s.beginGroup(ScrobblerSettings::kSettingsGroup);
    s.setValue(ScrobblerSettings::kEnabled, true);
    s.setValue(ScrobblerSettings::kOffline, false);
    s.setValue(ScrobblerSettings::kSubmit, 0);
    s.endGroup();

    // A logged in OAuth session, custom credentials so the test doesn't depend on the credentials compiled in, and the user token the requests are authorized with.
    s.remove(QLatin1String(ListenBrainzScrobbler::kSettingsGroup));
    s.beginGroup(ListenBrainzScrobbler::kSettingsGroup);
    s.setValue(ScrobblerSettings::kEnabled, true);
    s.setValue(ScrobblerSettings::kUserToken, u"user_token"_s);
    s.setValue(ListenBrainzSettings::kUseCustomApiCredentials, true);
    s.setValue(ListenBrainzSettings::kClientId, u"client_id"_s);
    s.setValue(ListenBrainzSettings::kClientSecret, u"client_secret"_s);
    s.setValue("token_type", u"Bearer"_s);
    s.setValue("access_token", u"access_token"_s);
    s.setValue("expires_in", 3600);
    s.setValue("login_time", QDateTime::currentSecsSinceEpoch());
    s.endGroup();

    settings_ = make_shared<ScrobblerSettingsService>();
    network_ = make_shared<ControlledNetworkAccessManager>();
    scrobbler_ = std::make_unique<ListenBrainzScrobbler>(settings_, network_);
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

  static Song MakeSong() {
    Song song(Song::Source::LocalFile);
    song.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 180 * kNsecPerSecTest);
    song.set_url(QUrl(u"file:///music/title.mp3"_s));
    return song;
  }

  static void SetUserToken(const QString &user_token) {
    Settings s;
    s.beginGroup(ListenBrainzScrobbler::kSettingsGroup);
    s.setValue(ScrobblerSettings::kUserToken, user_token);
    s.endGroup();
  }

  bool IsRequest(const qsizetype i, const QString &path) const {
    return network_->urls.value(i).path().endsWith(path);
  }

  static QString AccessTokenInSettings() {
    Settings s;
    s.beginGroup(ListenBrainzScrobbler::kSettingsGroup);
    const QString access_token = s.value("access_token").toString();
    s.endGroup();
    return access_token;
  }

  static constexpr qint64 kNsecPerSecTest = 1000000000LL;
  static constexpr char kOkReply[] = R"({"status": "ok"})";
  static constexpr char kUnauthorizedReply[] = R"({"code": 401, "error": "Invalid authorization token."})";

  QString organization_name_;
  QString cache_path_;
  SharedPtr<ScrobblerSettingsService> settings_;
  SharedPtr<ControlledNetworkAccessManager> network_;
  std::unique_ptr<ListenBrainzScrobbler> scrobbler_;
};

// Regression test: a rejected user token cleared the OAuth session, although the requests are authorized with the user token, so the user had to log in again without it helping.
TEST_F(ListenBrainzScrobblerTest, RejectedUserTokenKeepsOAuthSession) {

  scrobbler_->UpdateNowPlaying(MakeSong());
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, 401, QByteArray(kUnauthorizedReply));
  QCoreApplication::processEvents();

  // Nothing is sent until the user token is checked.
  EXPECT_FALSE(scrobbler_->authenticated());
  EXPECT_EQ(u"access_token"_s, AccessTokenInSettings());

  // Reloading the settings, for example after the user token was changed, tries the user token again.
  scrobbler_->ReloadSettings();
  EXPECT_TRUE(scrobbler_->authenticated());

}

// Regression test: a listen rejected because of the user token was removed from the cache, so it was never scrobbled.
TEST_F(ListenBrainzScrobblerTest, RejectedUserTokenKeepsListen) {

  const Song song = MakeSong();
  scrobbler_->UpdateNowPlaying(song);
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, 200, QByteArray(kOkReply));

  // The listen is submitted after the minimum submit delay.
  scrobbler_->Scrobble(song);
  ASSERT_TRUE(network_->WaitForRequests(2, 15000));
  ASSERT_TRUE(IsRequest(1, u"/submit-listens"_s));
  network_->FinishReply(1, 401, QByteArray(kUnauthorizedReply));
  QCoreApplication::processEvents();

  // The listen is submitted again when the user token was fixed, and removed from the cache when it was accepted.
  scrobbler_->ReloadSettings();
  ASSERT_TRUE(network_->WaitForRequests(3, 15000));
  ASSERT_TRUE(IsRequest(2, u"/submit-listens"_s));
  network_->FinishReply(2, 200, QByteArray(kOkReply));
  QCoreApplication::processEvents();

  scrobbler_->WriteCache();
  ScrobblerCache cache(QLatin1String(kCacheFile), nullptr);
  EXPECT_EQ(0, cache.Count());

}

// Regression test: a request sent with the previous user token, which was rejected after the settings were reloaded with a new user token, rejected the new user token.
TEST_F(ListenBrainzScrobblerTest, RejectedPreviousUserTokenKeepsNewUserToken) {

  scrobbler_->UpdateNowPlaying(MakeSong());
  ASSERT_TRUE(network_->WaitForRequests(1));

  SetUserToken(u"new_user_token"_s);
  scrobbler_->ReloadSettings();
  network_->FinishReply(0, 401, QByteArray(kUnauthorizedReply));
  QCoreApplication::processEvents();

  EXPECT_TRUE(scrobbler_->authenticated());

}

// Regression test: a love rejected because of the user token counted as a failed attempt, so it was discarded when it had failed before.
TEST_F(ListenBrainzScrobblerTest, RejectedUserTokenKeepsLove) {

  Song song = MakeSong();
  song.set_musicbrainz_recording_id(u"recording_mbid"_s);
  scrobbler_->UpdateNowPlaying(song);
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, 200, QByteArray(kOkReply));

  // Two temporary failures, then the user token is rejected.
  scrobbler_->Love();
  for (qsizetype i = 1; i <= 3; ++i) {
    ASSERT_TRUE(network_->WaitForRequests(i + 1));
    ASSERT_TRUE(IsRequest(i, u"/recording-feedback"_s));
    network_->FinishReply(i, i < 3 ? 500 : 401, i < 3 ? QByteArray() : QByteArray(kUnauthorizedReply));
    QCoreApplication::processEvents();
  }

  // The love is sent again when the user token was fixed.
  scrobbler_->ReloadSettings();
  ASSERT_TRUE(network_->WaitForRequests(5));
  EXPECT_TRUE(IsRequest(4, u"/recording-feedback"_s));

}

}  // namespace
