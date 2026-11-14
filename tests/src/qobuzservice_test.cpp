#include "gtest_include.h"
#include "gmock_include.h"

#include <memory>

#include <QDesktopServices>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

#include "constants/qobuzsettings.h"
#include "core/memorydatabase.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "core/urlhandlers.h"
#include "qobuz/qobuzservice.h"
#include "mock_networkaccessmanager.h"
#include "qobuzservice_test.h"

using namespace Qt::Literals::StringLiterals;
using testing::_;

namespace {

class QobuzTestNetwork : public NetworkAccessManager {
 public:
  MOCK_METHOD(QNetworkReply*, createRequest, (Operation, const QNetworkRequest&, QIODevice*), (override));
};

class QobuzServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    Settings settings;
    settings.beginGroup(QobuzSettings::kSettingsGroup);
    settings.remove(QString());
    settings.setValue(QobuzSettings::kAppId, u"test-app"_s);
    settings.setValue(QobuzSettings::kAppSecret, u"test-secret"_s);
    settings.setValue(QobuzSettings::kPrivateKey, u"test-private-key"_s);
    settings.sync();
    QDesktopServices::setUrlHandler(u"https"_s, &browser_, "Open");
    database_ = std::make_shared<MemoryDatabase>(nullptr);
    network_ = std::make_shared<QobuzTestNetwork>();
    handlers_ = std::make_shared<UrlHandlers>();
    service_ = std::make_unique<QobuzService>(nullptr, database_, network_, handlers_, nullptr);
  }

  void TearDown() override {
    QDesktopServices::unsetUrlHandler(u"https"_s);
  }

  void Start() {
    service_->Authenticate();
    ASSERT_EQ(QUrl(u"https://www.qobuz.com/signin/oauth"_s).host(), browser_.url.host());
    const QUrlQuery query(browser_.url);
    EXPECT_EQ(u"test-app"_s, query.queryItemValue(u"ext_app_id"_s));
    redirect_ = QUrl(query.queryItemValue(u"redirect_url"_s, QUrl::FullyDecoded));
    ASSERT_EQ(u"127.0.0.1"_s, redirect_.host());
    ASSERT_GT(redirect_.port(), 0);
  }

  void Redirect(const QByteArray &query) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, redirect_.port());
    ASSERT_TRUE(socket.waitForConnected());
    socket.write("GET /?" + query + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ASSERT_TRUE(QTest::qWaitFor([&socket]() { return socket.bytesAvailable() > 0; }));
  }

  QobuzTestBrowser browser_;
  QUrl redirect_;
  SharedPtr<MemoryDatabase> database_;
  SharedPtr<QobuzTestNetwork> network_;
  SharedPtr<UrlHandlers> handlers_;
  std::unique_ptr<QobuzService> service_;
};

TEST_F(QobuzServiceTest, CallbackExchangesCodeAndPersistsSession) {

  auto *reply = new MockNetworkReply(R"({"token":"test-token","userId":7})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _))
      .WillOnce([reply](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice*) {
        EXPECT_EQ(u"/api.json/0.2/oauth/callback"_s, request.url().path());
        const QUrlQuery query(request.url());
        EXPECT_EQ(u"test-code"_s, query.queryItemValue(u"code"_s));
        EXPECT_EQ(u"test-private-key"_s, query.queryItemValue(u"private_key"_s));
        EXPECT_EQ(QByteArray("test-app"), request.rawHeader("X-App-Id"));
        return reply;
      });
  QSignalSpy finished(service_.get(), &StreamingService::LoginFinished);
  Start();
  Redirect("code_autorisation=test-code");
  EXPECT_TRUE(finished.isEmpty());
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_TRUE(finished.first().first().toBool());
  EXPECT_TRUE(service_->authenticated());
  service_->ReloadSettings();
  EXPECT_EQ(u"test-token"_s, service_->user_auth_token());
  EXPECT_EQ(7, service_->user_id());
  service_->ClearSession();
  service_->ReloadSettings();
  EXPECT_FALSE(service_->authenticated());

}

TEST_F(QobuzServiceTest, DenialDoesNotExchangeCodeAndAllowsRetry) {

  EXPECT_CALL(*network_, createRequest(_, _, _)).Times(0);
  QSignalSpy finished(service_.get(), &StreamingService::LoginFinished);
  Start();
  Redirect("error=access_denied&error_description=Login%20denied");
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_EQ(u"Login denied"_s, finished.first().at(1).toString());
  EXPECT_FALSE(service_->authenticated());

  Start();
  Redirect("code=");
  ASSERT_EQ(2, finished.count());
  EXPECT_FALSE(finished.last().first().toBool());
  EXPECT_FALSE(finished.last().at(1).toString().isEmpty());

}

TEST_F(QobuzServiceTest, MissingTokenDoesNotReusePreviousSession) {

  Settings settings;
  settings.beginGroup(QobuzSettings::kSettingsGroup);
  settings.setValue(QobuzSettings::kUserAuthToken, u"previous-token"_s);
  settings.setValue(QobuzSettings::kUserId, 7);
  service_->ReloadSettings();

  auto *reply = new MockNetworkReply(R"({"userId":8})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
  QSignalSpy finished(service_.get(), &StreamingService::LoginFinished);
  Start();
  Redirect("code=test-code");
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_EQ(u"previous-token"_s, service_->user_auth_token());
  EXPECT_EQ(7, service_->user_id());

}

TEST_F(QobuzServiceTest, AcceptsLegacyTokenAndUserObject) {

  auto *reply = new MockNetworkReply(R"({"user_auth_token":"test-token","user":{"id":"7"}})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
  QSignalSpy finished(service_.get(), &StreamingService::LoginFinished);
  Start();
  Redirect("code=test-code");
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_TRUE(finished.first().first().toBool());
  EXPECT_TRUE(service_->authenticated());
  EXPECT_EQ(7, service_->user_id());

}

TEST_F(QobuzServiceTest, MissingOrInvalidUserIdDoesNotPreventLogin) {

  QSignalSpy finished(service_.get(), &StreamingService::LoginFinished);
  for (const QByteArray &response : {QByteArray(R"({"token":"test-token"})"),
                                     QByteArray(R"({"token":"test-token","userId":0})")}) {
    auto *reply = new MockNetworkReply(response, network_.get());
    reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
    Start();
    Redirect("code=test-code");
    reply->Done();
    ASSERT_FALSE(finished.isEmpty());
    EXPECT_TRUE(finished.last().first().toBool());
    EXPECT_TRUE(service_->authenticated());
    EXPECT_EQ(-1, service_->user_id());
    service_->ReloadSettings();
    EXPECT_TRUE(service_->authenticated());
    EXPECT_EQ(-1, service_->user_id());
    service_->ClearSession();
  }
  EXPECT_EQ(2, finished.count());

}

}  // namespace
