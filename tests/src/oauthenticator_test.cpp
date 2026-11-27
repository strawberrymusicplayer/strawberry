#include "gtest_include.h"
#include "gmock_include.h"

#include <QIODevice>
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QUrlQuery>

#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "includes/shared_ptr.h"
#include "mock_networkaccessmanager.h"
#include "oauthenticator_test.h"

using namespace Qt::Literals::StringLiterals;
using testing::_;

namespace {

class OAuthTestNetwork : public NetworkAccessManager {
 public:
  MOCK_METHOD(QNetworkReply*, createRequest, (Operation, const QNetworkRequest&, QIODevice*), (override));
};

class ScopedAuthorizationUrlHandler {
 public:
  explicit ScopedAuthorizationUrlHandler(QObject *receiver) {
    QDesktopServices::setUrlHandler(u"https"_s, receiver, "Open");
  }
  ~ScopedAuthorizationUrlHandler() { QDesktopServices::unsetUrlHandler(u"https"_s); }
};

TEST(OAuthenticatorTest, ClientCredentialsPersistAndLogoutClearsSession) {

  auto network = std::make_shared<OAuthTestNetwork>();
  OAuthenticator oauth(network);
  oauth.set_settings_group(u"OAuthClientCredentialsTest"_s);
  oauth.set_type(OAuthenticator::Type::Client_Credentials);
  oauth.set_client_id(u"test-client"_s);
  oauth.set_client_secret(u"test-secret"_s);
  oauth.set_access_token_url(QUrl(u"http://localhost/token"_s));
  auto *reply = new MockNetworkReply(R"({"token_type":"Bearer","access_token":"test-token"})", network.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network, createRequest(QNetworkAccessManager::PostOperation, _, _))
      .WillOnce([reply](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice *data) {
        EXPECT_EQ(QUrl(u"http://localhost/token"_s), request.url());
        EXPECT_EQ(QByteArray("Basic dGVzdC1jbGllbnQ6dGVzdC1zZWNyZXQ="), request.rawHeader("Authorization"));
        EXPECT_EQ(u"client_credentials"_s, QUrlQuery(QString::fromUtf8(data->readAll())).queryItemValue(u"grant_type"_s));
        return reply;
      });
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  reply->Done();
  ASSERT_EQ(1, finished.count());
  EXPECT_TRUE(finished.first().first().toBool());
  EXPECT_TRUE(oauth.authenticated());

  OAuthenticator restored(network);
  restored.set_settings_group(u"OAuthClientCredentialsTest"_s);
  restored.LoadSession();
  EXPECT_EQ(QByteArray("Bearer test-token"), restored.authorization_header());
  restored.ClearSession();
  EXPECT_FALSE(restored.authenticated());
  oauth.LoadSession();
  EXPECT_FALSE(oauth.authenticated());

}

TEST(OAuthenticatorTest, ClientCredentialsReportMissingSecret) {

  auto network = std::make_shared<OAuthTestNetwork>();
  EXPECT_CALL(*network, createRequest(_, _, _)).Times(0);
  OAuthenticator oauth(network);
  oauth.set_type(OAuthenticator::Type::Client_Credentials);
  oauth.set_client_id(u"test-client"_s);
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_FALSE(finished.first().at(1).toString().isEmpty());

}

TEST(OAuthenticatorTest, AuthorizationCodeRequiresActiveMatchingState) {

  auto network = std::make_shared<OAuthTestNetwork>();
  EXPECT_CALL(*network, createRequest(_, _, _)).Times(0);
  OAuthTestBrowser browser;
  ScopedAuthorizationUrlHandler handler(&browser);

  OAuthenticator oauth(network);
  oauth.set_authorize_url(QUrl(u"https://auth.example/authorize"_s));
  oauth.set_redirect_url(QUrl(u"tidal://login/auth"_s));
  oauth.set_use_local_redirect_server(false);
  oauth.set_client_id(u"test-client"_s);
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  ASSERT_EQ(u"https://auth.example/authorize"_s, browser.url.toString(QUrl::RemoveQuery));
  const QString state = QUrlQuery(browser.url).queryItemValue(u"state"_s);
  ASSERT_FALSE(state.isEmpty());

  oauth.ExternalAuthorizationUrlReceived(QUrl(u"tidal://login/auth?code=test-code&state=wrong"_s));
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_TRUE(finished.first().at(1).toString().contains(u"wrong state"_s));

  oauth.ExternalAuthorizationUrlReceived(QUrl(u"tidal://login/auth?code=test-code&state="_s + state));
  ASSERT_EQ(2, finished.count());
  EXPECT_FALSE(finished.last().first().toBool());
  EXPECT_TRUE(finished.last().at(1).toString().contains(u"unexpected"_s));

}

TEST(OAuthenticatorTest, AuthorizationDenialRequiresMatchingState) {

  auto network = std::make_shared<OAuthTestNetwork>();
  EXPECT_CALL(*network, createRequest(_, _, _)).Times(0);
  OAuthTestBrowser browser;
  ScopedAuthorizationUrlHandler handler(&browser);

  OAuthenticator oauth(network);
  oauth.set_authorize_url(QUrl(u"https://auth.example/authorize"_s));
  oauth.set_redirect_url(QUrl(u"tidal://login/auth"_s));
  oauth.set_use_local_redirect_server(false);
  oauth.set_client_id(u"test-client"_s);
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();

  oauth.ExternalAuthorizationUrlReceived(QUrl(u"tidal://login/auth?error=access_denied&state=wrong"_s));
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_TRUE(finished.first().at(1).toString().contains(u"wrong state"_s));

}

TEST(OAuthenticatorTest, AuthorizationDenialWithMatchingStateIsReported) {

  auto network = std::make_shared<OAuthTestNetwork>();
  EXPECT_CALL(*network, createRequest(_, _, _)).Times(0);
  OAuthTestBrowser browser;
  ScopedAuthorizationUrlHandler handler(&browser);

  OAuthenticator oauth(network);
  oauth.set_authorize_url(QUrl(u"https://auth.example/authorize"_s));
  oauth.set_redirect_url(QUrl(u"tidal://login/auth"_s));
  oauth.set_use_local_redirect_server(false);
  oauth.set_client_id(u"test-client"_s);
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  const QString state = QUrlQuery(browser.url).queryItemValue(u"state"_s);

  oauth.ExternalAuthorizationUrlReceived(QUrl(u"tidal://login/auth?error=access_denied&state="_s + state));
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_EQ(u"access_denied"_s, finished.first().at(1).toString());

}

TEST(OAuthenticatorTest, AuthorizationCodeExchangesOnlyForMatchingState) {

  auto network = std::make_shared<OAuthTestNetwork>();
  OAuthTestBrowser browser;
  ScopedAuthorizationUrlHandler handler(&browser);
  OAuthenticator oauth(network);
  oauth.set_authorize_url(QUrl(u"https://auth.example/authorize"_s));
  oauth.set_redirect_url(QUrl(u"tidal://login/auth"_s));
  oauth.set_use_local_redirect_server(false);
  oauth.set_client_id(u"test-client"_s);
  oauth.set_access_token_url(QUrl(u"https://auth.example/token"_s));

  auto *reply = new MockNetworkReply(R"({"token_type":"Bearer","access_token":"test-token"})", network.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network, createRequest(QNetworkAccessManager::PostOperation, _, _))
      .WillOnce([reply](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice *data) {
        EXPECT_EQ(QUrl(u"https://auth.example/token"_s), request.url());
        const QUrlQuery body(QString::fromUtf8(data->readAll()));
        EXPECT_EQ(u"authorization_code"_s, body.queryItemValue(u"grant_type"_s));
        EXPECT_EQ(u"test-code"_s, body.queryItemValue(u"code"_s));
        EXPECT_EQ(u"tidal://login/auth"_s, body.queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded));
        EXPECT_FALSE(body.queryItemValue(u"code_verifier"_s).isEmpty());
        return reply;
      });

  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  const QString state = QUrlQuery(browser.url).queryItemValue(u"state"_s);
  ASSERT_FALSE(state.isEmpty());

  QUrl callback(u"tidal://login/auth"_s);
  QUrlQuery callback_query;
  callback_query.addQueryItem(u"code"_s, u"test-code"_s);
  callback_query.addQueryItem(u"state"_s, state);
  callback.setQuery(callback_query);
  oauth.ExternalAuthorizationUrlReceived(callback);
  reply->Done();

  ASSERT_EQ(1, finished.count());
  EXPECT_TRUE(finished.first().first().toBool());
  EXPECT_TRUE(oauth.authenticated());

}

TEST(OAuthenticatorTest, CancellingAuthorizationIgnoresLateTokenReply) {

  auto network = std::make_shared<OAuthTestNetwork>();
  OAuthTestBrowser browser;
  ScopedAuthorizationUrlHandler handler(&browser);
  OAuthenticator oauth(network);
  oauth.set_authorize_url(QUrl(u"https://auth.example/authorize"_s));
  oauth.set_redirect_url(QUrl(u"tidal://login/auth"_s));
  oauth.set_use_local_redirect_server(false);
  oauth.set_client_id(u"test-client"_s);
  oauth.set_access_token_url(QUrl(u"https://auth.example/token"_s));

  auto *reply = new MockNetworkReply(R"({"token_type":"Bearer","access_token":"test-token"})", network.get());
  reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
  EXPECT_CALL(*network, createRequest(QNetworkAccessManager::PostOperation, _, _)).WillOnce(testing::Return(reply));
  EXPECT_CALL(*reply, abort()).Times(1);

  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  const QString state = QUrlQuery(browser.url).queryItemValue(u"state"_s);
  QUrl callback(u"tidal://login/auth"_s);
  QUrlQuery callback_query;
  callback_query.addQueryItem(u"code"_s, u"test-code"_s);
  callback_query.addQueryItem(u"state"_s, state);
  callback.setQuery(callback_query);
  oauth.ExternalAuthorizationUrlReceived(callback);

  oauth.CancelAuthentication();
  reply->Done();
  EXPECT_TRUE(finished.isEmpty());
  EXPECT_FALSE(oauth.authenticated());

}

#ifdef Q_OS_ANDROID
TEST(OAuthenticatorTest, BrowserAuthenticationIsRejectedBeforeOpeningBrowser) {

  auto network = std::make_shared<OAuthTestNetwork>();
  EXPECT_CALL(*network, createRequest(_, _, _)).Times(0);
  OAuthenticator oauth(network);
  oauth.set_type(OAuthenticator::Type::Authorization_Code);
  QSignalSpy finished(&oauth, &OAuthenticator::AuthenticationFinished);
  oauth.Authenticate();
  ASSERT_EQ(1, finished.count());
  EXPECT_FALSE(finished.first().first().toBool());
  EXPECT_TRUE(finished.first().at(1).toString().contains(u"Android"_s));
  oauth.ExternalAuthorizationUrlReceived(QUrl(u"tidal://login/auth?code=test-code&state="_s));
  ASSERT_EQ(2, finished.count());
  EXPECT_FALSE(finished.last().first().toBool());
  EXPECT_TRUE(finished.last().at(1).toString().contains(u"Android"_s));

}
#endif

}  // namespace
