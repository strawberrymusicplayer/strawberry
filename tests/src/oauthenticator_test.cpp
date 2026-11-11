#include "gtest_include.h"
#include "gmock_include.h"

#include <QIODevice>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QUrlQuery>

#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "includes/shared_ptr.h"
#include "mock_networkaccessmanager.h"

using namespace Qt::Literals::StringLiterals;
using testing::_;

namespace {

class OAuthTestNetwork : public NetworkAccessManager {
 public:
  MOCK_METHOD(QNetworkReply*, createRequest, (Operation, const QNetworkRequest&, QIODevice*), (override));
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
