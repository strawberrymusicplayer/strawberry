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

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QIODevice>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/networkaccessmanager.h"
#include "constants/acoustidsettings.h"
#include "tagfetcher/acoustidclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

// Records when the requests are sent, and sends them to a closed local port instead of AcoustID, so they fail right away.
class RecordingNetworkAccessManager : public NetworkAccessManager {
 public:
  explicit RecordingNetworkAccessManager() { elapsed_timer_.start(); }

  QList<qint64> request_times_;

 protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {
    Q_UNUSED(network_request)
    request_times_ << elapsed_timer_.elapsed();
    return NetworkAccessManager::createRequest(op, QNetworkRequest(QUrl(u"http://127.0.0.1:9/"_s)), outgoing_data);
  }

 private:
  QElapsedTimer elapsed_timer_;
};

class AcoustidClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings s;
    s.beginGroup(QLatin1String(AcoustidSettings::kSettingsGroup));
    s.setValue(AcoustidSettings::kUseCustomApiKey, true);
    s.setValue(AcoustidSettings::kApiKey, u"testkey"_s);
    s.endGroup();
    network_ = make_shared<RecordingNetworkAccessManager>();
  }

  void TearDown() override {
    Settings s;
    s.remove(QLatin1String(AcoustidSettings::kSettingsGroup));
  }

  static bool WaitForFinished(QSignalSpy *spy, const int count) {
    QElapsedTimer timer;
    timer.start();
    while (spy->count() < count) {
      if (timer.elapsed() > 10000) return false;
      spy->wait(100);
    }
    return true;
  }

  SharedPtr<RecordingNetworkAccessManager> network_;
};

TEST_F(AcoustidClientTest, RequestsAreRateLimited) {

  AcoustidClient client(network_);
  QSignalSpy spy(&client, &AcoustidClient::Finished);

  for (int i = 0; i < 4; ++i) {
    QString error;
    ASSERT_TRUE(client.Start(i, u"fingerprint"_s, 180000, error)) << error.toStdString();
  }

  // Only the first request is sent right away.
  ASSERT_EQ(1, network_->request_times_.count());

  ASSERT_TRUE(WaitForFinished(&spy, 4));
  ASSERT_EQ(4, network_->request_times_.count());

  // AcoustID allows 3 requests per second, timers can fire a little early.
  for (qsizetype i = 1; i < network_->request_times_.count(); ++i) {
    EXPECT_GE(network_->request_times_.at(i) - network_->request_times_.at(i - 1), 330) << "request " << i;
  }

}

TEST_F(AcoustidClientTest, CancelRemovesQueuedRequest) {

  AcoustidClient client(network_);
  QSignalSpy spy(&client, &AcoustidClient::Finished);

  for (int i = 0; i < 3; ++i) {
    QString error;
    ASSERT_TRUE(client.Start(i, u"fingerprint"_s, 180000, error)) << error.toStdString();
  }
  client.Cancel(1);

  ASSERT_TRUE(WaitForFinished(&spy, 2));
  // Wait for the time the cancelled request would have been sent.
  spy.wait(1000);

  EXPECT_EQ(2, network_->request_times_.count());
  ASSERT_EQ(2, spy.count());
  EXPECT_EQ(0, spy.at(0).at(0).toInt());
  EXPECT_EQ(2, spy.at(1).at(0).toInt());

}

}  // namespace
