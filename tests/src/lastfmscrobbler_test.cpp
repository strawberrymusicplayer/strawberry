#include "gtest_include.h"
#include "gmock_include.h"

#include <memory>

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHostAddress>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>

#include "constants/lastfmsettings.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "scrobbler/lastfmscrobbler.h"
#include "scrobbler/scrobblersettingsservice.h"
#include "mock_networkaccessmanager.h"
#include "oauthenticator_test.h"

using namespace Qt::Literals::StringLiterals;
using testing::_;

namespace {

class LastFMTestNetwork : public NetworkAccessManager {
 public:
  MOCK_METHOD(QNetworkReply*, createRequest, (Operation, const QNetworkRequest&, QIODevice*), (override));
};

class LastFMScrobblerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(cache_dir_.isValid());
    cache_was_set_ = qEnvironmentVariableIsSet("XDG_CACHE_HOME");
    original_cache_ = qgetenv("XDG_CACHE_HOME");
    ASSERT_TRUE(qputenv("XDG_CACHE_HOME", cache_dir_.path().toUtf8()));
    original_application_name_ = QCoreApplication::applicationName();
    original_organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"LastFMTest"_s);
    QCoreApplication::setApplicationName(QFileInfo(cache_dir_.path()).fileName());
    Settings settings;
    settings.beginGroup(LastFMScrobbler::kSettingsGroup);
    settings.remove(QString());
    settings.setValue(LastFMSettings::kUseCustomApiCredentials, true);
    settings.setValue(LastFMSettings::kClientId, u"test-key"_s);
    settings.setValue(LastFMSettings::kClientSecret, u"test-secret"_s);
    settings.endGroup();
    settings.sync();
    ASSERT_EQ(QSettings::NoError, settings.status());
    QDesktopServices::setUrlHandler(u"https"_s, &browser_, "Open");
    network_ = std::make_shared<LastFMTestNetwork>();
    settings_ = std::make_shared<ScrobblerSettingsService>();
    service_ = std::make_unique<LastFMScrobbler>(settings_, network_);
  }

  void TearDown() override {
    service_.reset();
    QDesktopServices::unsetUrlHandler(u"https"_s);
    QCoreApplication::setApplicationName(original_application_name_);
    QCoreApplication::setOrganizationName(original_organization_name_);
    if (cache_was_set_) qputenv("XDG_CACHE_HOME", original_cache_);
    else qunsetenv("XDG_CACHE_HOME");
  }

  void Start() {
    browser_.url.clear();
    QTimer::singleShot(0, []() {
      auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      ASSERT_NE(nullptr, box);
      box->button(QMessageBox::Open)->click();
    });
    service_->Authenticate();
    ASSERT_EQ(u"www.last.fm"_s, browser_.url.host());
    QUrlQuery query(browser_.url);
    EXPECT_EQ(u"test-key"_s, query.queryItemValue(u"api_key"_s));
    redirect_ = QUrl(query.queryItemValue(u"cb"_s, QUrl::FullyDecoded));
    ASSERT_GT(redirect_.port(), 0);
  }

  void Redirect(const QByteArray &query) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, redirect_.port());
    ASSERT_TRUE(socket.waitForConnected());
    socket.write("GET " + redirect_.path().toUtf8() + "?" + query + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ASSERT_TRUE(QTest::qWaitFor([&socket]() { return socket.bytesAvailable() > 0; }));
  }

  OAuthTestBrowser browser_;
  QTemporaryDir cache_dir_;
  QByteArray original_cache_;
  QString original_application_name_;
  QString original_organization_name_;
  bool cache_was_set_ = false;
  QUrl redirect_;
  SharedPtr<LastFMTestNetwork> network_;
  SharedPtr<ScrobblerSettingsService> settings_;
  std::unique_ptr<LastFMScrobbler> service_;
};

TEST_F(LastFMScrobblerTest, CallbackExchangesTokenAndPersistsSession) {

  auto *reply = new MockNetworkReply(R"({"session":{"subscriber":false,"name":"test-user","key":"test-session"}})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _))
      .WillOnce([reply](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice*) {
        QUrlQuery query(request.url());
        EXPECT_EQ(u"auth.getSession"_s, query.queryItemValue(u"method"_s));
        EXPECT_EQ(u"test-token"_s, query.queryItemValue(u"token"_s));
        EXPECT_FALSE(query.queryItemValue(u"api_sig"_s).isEmpty());
        return reply;
      });
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Start();
  Redirect("token=test-token");
  EXPECT_TRUE(finished.isEmpty());
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_TRUE(finished.first().first().toBool());
  EXPECT_TRUE(service_->authenticated());
  service_->LoadSession();
  EXPECT_EQ(u"test-user"_s, service_->username());
  service_->ClearSession();
  service_->LoadSession();
  EXPECT_FALSE(service_->authenticated());

}

TEST_F(LastFMScrobblerTest, DenialAndEmptyTokenFailAndPermitRetry) {

  EXPECT_CALL(*network_, createRequest(_, _, _)).Times(0);
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Start();
  Redirect("error=access_denied&token=must-not-be-used");
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.last().first().toBool());
  EXPECT_EQ(u"access_denied"_s, finished.last().at(1).toString());
  Start();
  Redirect("token=");
  ASSERT_EQ(2, finished.count());
  EXPECT_FALSE(finished.last().first().toBool());
  EXPECT_FALSE(finished.last().at(1).toString().isEmpty());

}

TEST_F(LastFMScrobblerTest, CancelAbortsSessionExchangeAndIgnoresLateReply) {

  auto *reply = new MockNetworkReply(R"({"session":{"subscriber":false,"name":"test-user","key":"test-session"}})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
  EXPECT_CALL(*reply, abort()).Times(1);
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Start();
  Redirect("token=test-token");
  service_->CancelAuthentication();
  reply->Done();
  EXPECT_TRUE(finished.isEmpty());
  EXPECT_FALSE(service_->authenticated());

}

TEST_F(LastFMScrobblerTest, CancelClosesListenerAndAllowsRetry) {

  EXPECT_CALL(*network_, createRequest(_, _, _)).Times(0);
  Start();
  const int old_port = redirect_.port();
  service_->CancelAuthentication();
  QTcpSocket socket;
  socket.connectToHost(QHostAddress::LocalHost, old_port);
  EXPECT_FALSE(socket.waitForConnected(1000));
  Start();
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Redirect("error=access_denied");
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());

}

TEST_F(LastFMScrobblerTest, LogoutAbortsSessionExchange) {

  auto *reply = new MockNetworkReply(R"({"session":{"subscriber":false,"name":"test-user","key":"test-session"}})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
  EXPECT_CALL(*reply, abort()).Times(1);
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Start();
  Redirect("token=test-token");
  service_->ClearSession();
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_FALSE(service_->authenticated());
  service_->LoadSession();
  EXPECT_FALSE(service_->authenticated());

}

TEST_F(LastFMScrobblerTest, EmptySessionDoesNotOverwriteExistingLogin) {

  Settings settings;
  settings.beginGroup(LastFMScrobbler::kSettingsGroup);
  settings.setValue(u"username"_s, u"existing-user"_s);
  settings.setValue(u"session_key"_s, u"existing-session"_s);
  settings.endGroup();
  service_->LoadSession();
  auto *reply = new MockNetworkReply(R"({"session":{"subscriber":false,"name":"","key":""}})", network_.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network_, createRequest(QNetworkAccessManager::GetOperation, _, _)).WillOnce(testing::Return(reply));
  QSignalSpy finished(service_.get(), &LastFMScrobbler::AuthenticationComplete);
  Start();
  Redirect("token=test-token");
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_EQ(u"existing-user"_s, service_->username());
  EXPECT_TRUE(service_->authenticated());

}

}  // namespace
