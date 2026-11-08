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

#include <QList>
#include <QPointer>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QSignalSpy>
#include <QTest>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/networkaccessmanager.h"
#include "constants/discogssettings.h"
#include "constants/opentidalsettings.h"
#include "covermanager/discogscoverprovider.h"
#include "covermanager/opentidalcoverprovider.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// A reply which is finished by the test.
class ControlledNetworkReply : public QNetworkReply {
 public:
  explicit ControlledNetworkReply(const QNetworkRequest &network_request, QObject *parent = nullptr) : QNetworkReply(parent), pos_(0) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
  }

  void abort() override {
    if (isFinished()) return;
    setError(QNetworkReply::OperationCanceledError, u"Operation canceled"_s);
    setFinished(true);
    Q_EMIT finished();
  }

  void Finish(const QByteArray &data) {
    if (isFinished()) return;
    data_ = data;
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
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
  // The URLs are kept, the replies are deleted when they're handled.
  QList<QUrl> urls;

  // The paths of the requests sent, to tell which kind of request it is.
  QStringList Paths() const {
    QStringList paths;
    for (const QUrl &url : urls) {
      paths << url.path();
    }
    return paths;
  }

  bool WaitForRequests(const int count) const {
    return QTest::qWaitFor([this, count]() { return replies.count() >= count; }, 5000) && replies.count() == count;
  }

  void FinishReply(const int i, const QByteArray &data) {
    if (replies.value(i)) replies.value(i)->Finish(data);
  }

  // The decoded queries of the requests sent, to tell which search a request is for.
  QStringList Queries() const {
    QStringList queries;
    for (const QUrl &url : urls) {
      queries << QUrlQuery(url).toString(QUrl::FullyDecoded);
    }
    return queries;
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

// The settings are cleared, and custom credentials are set, so the tests don't depend on the credentials compiled in.
void SetCustomApiCredentials(const QString &settings_group, const QString &use_custom_api_credentials_key, const QString &client_id_key, const QString &client_secret_key) {

  Settings s;
  s.remove(settings_group);
  s.beginGroup(settings_group);
  s.setValue(use_custom_api_credentials_key, true);
  s.setValue(client_id_key, u"client_id"_s);
  s.setValue(client_secret_key, u"client_secret"_s);
  s.endGroup();

}

// Regression test: canceling a search left its requests in the queue, they were still sent, one each second, before the requests for the next searches.
TEST(CoverProviderCancelTest, DiscogsCanceledSearchIsNotSent) {

  SetCustomApiCredentials(u"Discogs"_s, QLatin1String(DiscogsSettings::kUseCustomApiCredentials), QLatin1String(DiscogsSettings::kClientId), QLatin1String(DiscogsSettings::kClientSecret));

  SharedPtr<ControlledNetworkAccessManager> network = make_shared<ControlledNetworkAccessManager>();
  DiscogsCoverProvider provider(network);
  QSignalSpy spy(&provider, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider.StartSearch(u"Canceled Artist"_s, u"Canceled Album"_s, QString(), 1));
  ASSERT_TRUE(provider.StartSearch(u"Next Artist"_s, u"Next Album"_s, QString(), 2));
  provider.CancelSearch(1);

  ASSERT_TRUE(QTest::qWaitFor([&network]() { return !network->replies.isEmpty(); }, 5000));
  QTest::qWait(1500);

  const QStringList queries = network->Queries();
  ASSERT_EQ(1, queries.count()) << queries.join(u", "_s).toStdString();
  EXPECT_TRUE(queries.first().contains(u"next artist"_s)) << queries.first().toStdString();
  EXPECT_EQ(0, spy.count());

}

// Regression test: canceling a search left its release requests in the queue, they were sent before the requests for the next searches.
TEST(CoverProviderCancelTest, DiscogsCanceledReleasesAreNotSent) {

  SetCustomApiCredentials(u"Discogs"_s, QLatin1String(DiscogsSettings::kUseCustomApiCredentials), QLatin1String(DiscogsSettings::kClientId), QLatin1String(DiscogsSettings::kClientSecret));

  SharedPtr<ControlledNetworkAccessManager> network = make_shared<ControlledNetworkAccessManager>();
  DiscogsCoverProvider provider(network);
  QSignalSpy spy(&provider, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider.StartSearch(u"Canceled Artist"_s, u"Canceled Album"_s, QString(), 1));
  ASSERT_TRUE(network->WaitForRequests(1));
  ASSERT_TRUE(provider.StartSearch(u"Next Artist"_s, u"Next Album"_s, QString(), 2));

  // The search reply queues a release request for each release, they're sent before the next search.
  network->FinishReply(0, R"({"results": [{"id": 101, "title": "Canceled Album", "resource_url": "https://api.discogs.com/masters/101"}, {"id": 102, "title": "Canceled Album", "resource_url": "https://api.discogs.com/masters/102"}]})");
  provider.CancelSearch(1);

  ASSERT_TRUE(QTest::qWaitFor([&network]() { return network->replies.count() >= 2; }, 5000));
  QTest::qWait(1500);

  const QStringList queries = network->Queries();
  ASSERT_EQ(2, queries.count()) << network->Paths().join(u", "_s).toStdString();
  EXPECT_TRUE(queries.last().contains(u"next artist"_s)) << queries.last().toStdString();
  EXPECT_EQ(0, spy.count());

}

class OpenTidalCoverProviderCancelTest : public ::testing::Test {
 protected:
  void SetUp() override {

    SetCustomApiCredentials(u"OpenTidal"_s, QLatin1String(OpenTidalSettings::kUseCustomApiCredentials), QLatin1String(OpenTidalSettings::kClientId), QLatin1String(OpenTidalSettings::kClientSecret));
    Settings s;
    s.beginGroup(u"OpenTidal"_s);
    s.setValue(u"token_type"_s, u"Bearer"_s);
    s.setValue(u"access_token"_s, u"token"_s);
    s.endGroup();

    network_ = make_shared<ControlledNetworkAccessManager>();
    provider_ = std::make_unique<OpenTidalCoverProvider>(network_);

  }

  void TearDown() override {
    provider_.reset();
    Settings s;
    s.remove(u"OpenTidal"_s);
  }

  SharedPtr<ControlledNetworkAccessManager> network_;
  std::unique_ptr<OpenTidalCoverProvider> provider_;
};

// Regression test: canceling a search did nothing, its requests were still sent before the requests for the next searches.
TEST_F(OpenTidalCoverProviderCancelTest, CanceledSearchIsNotSent) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Canceled Artist"_s, u"Canceled Album"_s, QString(), 1));
  ASSERT_TRUE(provider_->StartSearch(u"Next Artist"_s, u"Next Album"_s, QString(), 2));
  provider_->CancelSearch(1);

  ASSERT_TRUE(QTest::qWaitFor([this]() { return !network_->replies.isEmpty(); }, 5000));
  QTest::qWait(700);

  const QStringList queries = network_->Queries();
  ASSERT_EQ(1, queries.count()) << queries.join(u", "_s).toStdString();
  EXPECT_TRUE(queries.first().contains(u"Next Artist"_s)) << queries.first().toStdString();
  EXPECT_EQ(0, spy.count());

}

// Regression test: the reply for a canceled search was still handled, it queued a request for each album found, and the search reported its results.
TEST_F(OpenTidalCoverProviderCancelTest, ReplyAfterCancelAddsNoRequests) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Artist"_s, u"Album"_s, QString(), 1));
  ASSERT_TRUE(QTest::qWaitFor([this]() { return network_->replies.count() == 1; }, 5000));

  provider_->CancelSearch(1);
  ASSERT_TRUE(network_->replies.first());
  network_->replies.first()->Finish(R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}, {"id": "101", "type": "albums", "attributes": {"title": "Album"}}]})");
  QTest::qWait(1000);

  EXPECT_EQ(1, network_->replies.count()) << network_->Queries().join(u", "_s).toStdString();
  EXPECT_EQ(0, spy.count());

}

// Regression test: canceling a search left its album cover requests in the queue, they were sent before the requests for the next searches.
TEST_F(OpenTidalCoverProviderCancelTest, CanceledAlbumCoverRequestsAreNotSent) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Canceled Artist"_s, u"Canceled Album"_s, QString(), 1));
  ASSERT_TRUE(network_->WaitForRequests(1));
  ASSERT_TRUE(provider_->StartSearch(u"Next Artist"_s, u"Next Album"_s, QString(), 2));

  network_->FinishReply(0, R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}, {"id": "101", "type": "albums", "attributes": {"title": "Album"}}]})");
  provider_->CancelSearch(1);

  ASSERT_TRUE(QTest::qWaitFor([this]() { return network_->replies.count() >= 2; }, 5000));
  QTest::qWait(700);

  const QStringList paths = network_->Paths();
  ASSERT_EQ(2, paths.count()) << paths.join(u", "_s).toStdString();
  EXPECT_TRUE(paths.last().endsWith(u"/searchResults"_s)) << paths.last().toStdString();
  EXPECT_TRUE(network_->Queries().last().contains(u"Next Artist"_s)) << network_->Queries().last().toStdString();
  EXPECT_EQ(0, spy.count());

}

// Regression test: canceling a search left its artwork requests in the queue, they were sent before the requests for the next searches.
TEST_F(OpenTidalCoverProviderCancelTest, CanceledArtworkRequestsAreNotSent) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Canceled Artist"_s, u"Canceled Album"_s, QString(), 1));
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}]})");
  ASSERT_TRUE(network_->WaitForRequests(2));
  ASSERT_TRUE(provider_->StartSearch(u"Next Artist"_s, u"Next Album"_s, QString(), 2));

  network_->FinishReply(1, R"({"data": [{"id": "200", "type": "artworks"}, {"id": "201", "type": "artworks"}]})");
  provider_->CancelSearch(1);

  ASSERT_TRUE(QTest::qWaitFor([this]() { return network_->replies.count() >= 3; }, 5000));
  QTest::qWait(700);

  const QStringList paths = network_->Paths();
  ASSERT_EQ(3, paths.count()) << paths.join(u", "_s).toStdString();
  EXPECT_TRUE(paths.last().endsWith(u"/searchResults"_s)) << paths.last().toStdString();
  EXPECT_TRUE(network_->Queries().last().contains(u"Next Artist"_s)) << network_->Queries().last().toStdString();
  EXPECT_EQ(0, spy.count());

}

// Regression test: the album cover reply for a canceled search was still handled, it queued a request for each artwork.
TEST_F(OpenTidalCoverProviderCancelTest, AlbumCoverReplyAfterCancelAddsNoRequests) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Artist"_s, u"Album"_s, QString(), 1));
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}]})");
  ASSERT_TRUE(network_->WaitForRequests(2));

  provider_->CancelSearch(1);
  network_->FinishReply(1, R"({"data": [{"id": "200", "type": "artworks"}, {"id": "201", "type": "artworks"}]})");
  QTest::qWait(1000);

  EXPECT_EQ(2, network_->replies.count()) << network_->Paths().join(u", "_s).toStdString();
  EXPECT_EQ(0, spy.count());

}

// The artwork reply for a canceled search doesn't report the results.
TEST_F(OpenTidalCoverProviderCancelTest, ArtworkReplyAfterCancelIsNotReported) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Artist"_s, u"Album"_s, QString(), 1));
  ASSERT_TRUE(network_->WaitForRequests(1));
  network_->FinishReply(0, R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}]})");
  ASSERT_TRUE(network_->WaitForRequests(2));
  network_->FinishReply(1, R"({"data": [{"id": "200", "type": "artworks"}]})");
  ASSERT_TRUE(network_->WaitForRequests(3));

  provider_->CancelSearch(1);
  network_->FinishReply(2, R"({"data": {"attributes": {"files": [{"href": "https://example.com/cover.jpg", "meta": {"width": 1280, "height": 1280}}]}}})");
  QTest::qWait(700);

  EXPECT_EQ(3, network_->replies.count()) << network_->Paths().join(u", "_s).toStdString();
  EXPECT_EQ(0, spy.count());

}

// A search which is not canceled still requests the album covers and artwork, and reports its results when they're received.
TEST_F(OpenTidalCoverProviderCancelTest, SearchWithoutCancelIsFinished) {

  QSignalSpy spy(&*provider_, &CoverProvider::SearchFinished);

  ASSERT_TRUE(provider_->StartSearch(u"Artist"_s, u"Album"_s, QString(), 1));
  ASSERT_TRUE(QTest::qWaitFor([this]() { return network_->replies.count() == 1; }, 5000));
  network_->replies.first()->Finish(R"({"included": [{"id": "100", "type": "albums", "attributes": {"title": "Album"}}]})");

  ASSERT_TRUE(network_->WaitForRequests(2));
  network_->FinishReply(1, R"({"data": [{"id": "200", "type": "artworks"}]})");
  ASSERT_TRUE(network_->WaitForRequests(3));
  network_->FinishReply(2, R"({"data": {"attributes": {"files": [{"href": "https://example.com/cover.jpg", "meta": {"width": 1280, "height": 1280}}]}}})");

  ASSERT_TRUE(QTest::qWaitFor([&spy]() { return !spy.isEmpty(); }, 5000));
  ASSERT_EQ(1, spy.count());
  EXPECT_EQ(1, spy.first().value(0).toInt());
  EXPECT_EQ(1, spy.first().value(1).value<CoverProviderSearchResults>().count());

}

}  // namespace
