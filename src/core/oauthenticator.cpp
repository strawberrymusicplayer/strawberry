/*
 * Strawberry Music Player
 * Copyright 2022-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDesktopServices>
#include <QMessageBox>

#include "constants/timeconstants.h"
#include "utilities/randutils.h"
#include "logging.h"
#include "settings.h"
#include "networkaccessmanager.h"
#include "localredirectserver.h"
#include "oauthenticator.h"

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace {
constexpr char kTokenType[] = "token_type";
constexpr char kAccessToken[] = "access_token";
constexpr char kRefreshToken[] = "refresh_token";
constexpr char kExpiresIn[] = "expires_in";
constexpr char kLoginTime[] = "login_time";
constexpr char kUserId[] = "user_id";
constexpr char kCountryCode[] = "country_code";

// Refresh the access token this many seconds before it expires, so requests are not sent with an access token that is about to expire.
constexpr qint64 kRefreshMarginSecs = 60;

// Timers don't count the time the system is suspended, so check the expiry against the clock at least this often, instead of waiting for one long timer.
constexpr qint64 kMaxRefreshCheckIntervalSecs = 300;

// When refreshing the access token fails, for example because the network is not up yet after resuming, try again after this many seconds, doubled for each failure.
constexpr qint64 kMinRefreshRetryIntervalSecs = 60;
constexpr qint64 kMaxRefreshRetryIntervalSecs = 1800;

// Use at most half of the token lifetime as the margin, otherwise a token that expires within the margin would be refreshed again right away.
qint64 RefreshMarginSecs(const qint64 expires_in) {
  return std::min(kRefreshMarginSecs, expires_in / 2);
}

// OAuth errors from refreshing that are not temporary, so trying again with the same refresh token can't succeed.
bool IsTerminalRefreshError(const QString &error) {

  static const QStringList terminal_errors = QStringList() << u"invalid_grant"_s
                                                           << u"invalid_request"_s
                                                           << u"invalid_client"_s
                                                           << u"unauthorized_client"_s
                                                           << u"unsupported_grant_type"_s
                                                           << u"invalid_scope"_s
                                                           << u"access_denied"_s;

  return terminal_errors.contains(error, Qt::CaseInsensitive);

}

}  // namespace

OAuthenticator::OAuthenticator(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network),
      timer_refresh_login_(new QTimer(this)),
      type_(Type::Authorization_Code),
      use_local_redirect_server_(true),
      port_type_(PortType::SetToRedirectURL),
      use_pkce_(true),
      expires_in_(0LL),
      login_time_(0LL),
      user_id_(0),
      refresh_in_progress_(false),
      refresh_retry_interval_(0LL) {

  timer_refresh_login_->setSingleShot(true);
  QObject::connect(timer_refresh_login_, &QTimer::timeout, this, &OAuthenticator::RefreshLoginTimerTimeout);

}

OAuthenticator::~OAuthenticator() {

  AbortReplies();

}

void OAuthenticator::AbortReplies() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  refresh_in_progress_ = false;

}

void OAuthenticator::set_settings_group(const QString &settings_group) {

  settings_group_ = settings_group;

}

void OAuthenticator::set_type(const Type type) {

  type_ = type;

}

void OAuthenticator::set_authorize_url(const QUrl &authorize_url) {

  authorize_url_ = authorize_url;

}

void OAuthenticator::set_redirect_url(const QUrl &redirect_url) {

  redirect_url_ = redirect_url;

}

void OAuthenticator::set_access_token_url(const QUrl &access_token_url) {

  access_token_url_ = access_token_url;

}

void OAuthenticator::set_client_id(const QString &client_id) {

  client_id_ = client_id;

}

void OAuthenticator::set_client_secret(const QString &client_secret) {

  client_secret_ = client_secret;

}

void OAuthenticator::set_scope(const QString &scope) {

  scope_ = scope;

}

void OAuthenticator::set_use_local_redirect_server(const bool use_local_redirect_server) {

  use_local_redirect_server_ = use_local_redirect_server;

}

void OAuthenticator::set_port_type(const PortType port_type) {

  port_type_ = port_type;

}

void OAuthenticator::set_use_pkce(const bool use_pkce) {

  use_pkce_ = use_pkce;

}

QByteArray OAuthenticator::authorization_header() const {

  if (token_type_.isEmpty() || access_token_.isEmpty()) {
    return QByteArray();
  }

  return token_type().toUtf8() + " " + access_token().toUtf8();

}

QString OAuthenticator::GrantType() const {

  switch (type_) {
    case Type::Authorization_Code:
      return u"authorization_code"_s;
      break;
    case Type::Client_Credentials:
      return u"client_credentials"_s;
      break;
  }

  return QString();

}

void OAuthenticator::LoadSession() {

  Settings s;
  s.beginGroup(settings_group_);
  token_type_ = s.value(kTokenType).toString();
  access_token_ = s.value(kAccessToken).toString();
  refresh_token_ = s.value(kRefreshToken).toString();
  expires_in_ = s.value(kExpiresIn, 0LL).toLongLong();
  login_time_ = s.value(kLoginTime, 0LL).toLongLong();
  country_code_ = s.value(kCountryCode).toString();
  user_id_ = s.value(kUserId).toULongLong();
  s.endGroup();

  StartRefreshLoginTimer();

}

void OAuthenticator::ClearSession() {

  token_type_.clear();
  access_token_.clear();
  refresh_token_.clear();
  expires_in_ = 0;
  login_time_ = 0;
  country_code_.clear();
  user_id_ = 0;
  refresh_retry_interval_ = 0;

  // A pending reply would store the tokens again when it finishes, logging the user back in.
  AbortReplies();

  Settings s;
  s.beginGroup(settings_group_);
  s.remove(kTokenType);
  s.remove(kAccessToken);
  s.remove(kRefreshToken);
  s.remove(kExpiresIn);
  s.remove(kLoginTime);
  s.remove(kCountryCode);
  s.remove(kUserId);
  s.endGroup();

  if (timer_refresh_login_->isActive()) {
    timer_refresh_login_->stop();
  }

}

void OAuthenticator::StartRefreshLoginTimer() {

  if (login_time_ > 0 && !refresh_token_.isEmpty() && expires_in_ > 0) {
    const qint64 time = std::max(1LL, expires_in_ - RefreshMarginSecs(expires_in_) - (QDateTime::currentSecsSinceEpoch() - login_time_));
    qLog(Debug) << settings_group_ << "Refreshing login in" << time << "seconds";
    timer_refresh_login_->start(static_cast<int>(std::min(time, kMaxRefreshCheckIntervalSecs) * kMsecPerSec));
  }

}

void OAuthenticator::RefreshLoginTimerTimeout() {

  // Try again when refreshing failed, otherwise only refresh when the access token is about to expire according to the clock, and check again later if not.
  if (refresh_retry_interval_ > 0 || (expires_in_ > 0 && QDateTime::currentSecsSinceEpoch() - login_time_ >= expires_in_ - RefreshMarginSecs(expires_in_))) {
    RerefreshAccessToken();
  }
  else {
    StartRefreshLoginTimer();
  }

}

void OAuthenticator::ScheduleRefreshRetry() {

  if (refresh_token_.isEmpty()) return;

  refresh_retry_interval_ = refresh_retry_interval_ == 0 ? kMinRefreshRetryIntervalSecs : std::min(refresh_retry_interval_ * 2, kMaxRefreshRetryIntervalSecs);
  qLog(Debug) << settings_group_ << "Refreshing login failed, trying again in" << refresh_retry_interval_ << "seconds";
  timer_refresh_login_->start(static_cast<int>(refresh_retry_interval_ * kMsecPerSec));

}

bool OAuthenticator::RefreshAccessToken() {

  if (client_id_.isEmpty() || refresh_token_.isEmpty()) return false;

  // Keep the backoff when refreshing failed, the retry timer refreshes again.
  // Otherwise every request rejected with the old access token would refresh right away.
  if (refresh_retry_interval_ > 0 && timer_refresh_login_->isActive()) return true;

  RerefreshAccessToken();

  return true;

}

QUrl OAuthenticator::EffectiveRedirectUrl() const {

  QUrl redirect_url(redirect_url_);

  if (!local_redirect_server_.isNull()) {
    switch (port_type_) {
      case PortType::SetToRedirectURL:{
        redirect_url.setPort(local_redirect_server_->port());
        break;
      }
      case PortType::PassAsUrlParam:{
        QUrlQuery redirect_url_query(redirect_url);
        redirect_url_query.addQueryItem(u"port"_s, QString::number(local_redirect_server_->port()));
        redirect_url.setQuery(redirect_url_query);
        break;
      }
      case PortType::PassInState:{
        // The port travels in the "state" parameter instead.
        break;
      }
    }
  }

  return redirect_url;

}

void OAuthenticator::Authenticate() {

  if (client_id_.isEmpty()) {
    Q_EMIT AuthenticationFinished(false, tr("Missing client ID"));
    return;
  }

  if (type_ == Type::Client_Credentials) {
    if (client_secret_.isEmpty()) {
      Q_EMIT AuthenticationFinished(false, tr("Missing client secret"));
      return;
    }
    RequestAccessToken();
    return;
  }

  if (!redirect_url_.isValid()) {
    Q_EMIT AuthenticationFinished(false, tr("Invalid redirect URL"));
    return;
  }

  if (use_local_redirect_server_) {
    local_redirect_server_.reset(new LocalRedirectServer(this));
    const bool success = local_redirect_server_->Listen();
    if (!success) {
      Q_EMIT AuthenticationFinished(false, local_redirect_server_->error());
      local_redirect_server_.reset();
      return;
    }
    QObject::connect(&*local_redirect_server_, &LocalRedirectServer::Finished, this, &OAuthenticator::RedirectArrived);
  }

  const QUrl redirect_url = EffectiveRedirectUrl();

  state_ = Utilities::CryptographicRandomString(32);
  if (port_type_ == PortType::PassInState && !local_redirect_server_.isNull()) {
    state_ += u'-' + QString::number(local_redirect_server_->port());
  }

  if (use_pkce_) {
    code_verifier_ = Utilities::CryptographicRandomString(44);
    code_challenge_ = QString::fromLatin1(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  }
  else {
    code_verifier_.clear();
    code_challenge_.clear();
  }

  ParamList params = ParamList() << Param(u"response_type"_s, u"code"_s)
                                 << Param(u"redirect_uri"_s, redirect_url.toString())
                                 << Param(u"state"_s, state_);

  if (use_pkce_) {
    params << Param(u"code_challenge_method"_s, u"S256"_s)
           << Param(u"code_challenge"_s, code_challenge_);
  }

  if (!client_id_.isEmpty()) {
    params << Param(u"client_id"_s, client_id_);
  }

  if (!scope_.isEmpty()) {
    params << Param(u"scope"_s, scope_);
  }

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first, ";")), QString::fromLatin1(QUrl::toPercentEncoding(param.second, ";")));
  }

  QUrl url(authorize_url_);
  url.setQuery(url_query);

  const bool success = QDesktopServices::openUrl(url);
  if (!success) {
    QMessageBox messagebox(QMessageBox::Information, tr("Authentication"), tr("Please open this URL in your browser") + QStringLiteral(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void OAuthenticator::RedirectArrived() {

  if (local_redirect_server_.isNull()) {
    return;
  }

  if (local_redirect_server_->success()) {
    AuthorizationUrlReceived(local_redirect_server_->request_url(), EffectiveRedirectUrl());
  }
  else {
    Q_EMIT AuthenticationFinished(false, local_redirect_server_->error());
  }

  local_redirect_server_.reset();

}

void OAuthenticator::ExternalAuthorizationUrlReceived(const QUrl &request_url) {

  AuthorizationUrlReceived(request_url, redirect_url_);

}

void OAuthenticator::AuthorizationUrlReceived(const QUrl &request_url, const QUrl &redirect_url) {

  if (!request_url.isValid()) {
    Q_EMIT AuthenticationFinished(false, tr("Received invalid reply from web browser."));
    return;
  }

  if (!request_url.hasQuery()) {
    Q_EMIT AuthenticationFinished(false, tr("Redirect URL is missing query."));
    return;
  }

  QUrlQuery url_query(request_url);

  if (url_query.hasQueryItem(u"error_description"_s)) {
    Q_EMIT AuthenticationFinished(false, url_query.queryItemValue(u"error_description"_s, QUrl::FullyDecoded));
    return;
  }

  if (url_query.hasQueryItem(u"error"_s)) {
    Q_EMIT AuthenticationFinished(false, url_query.queryItemValue(u"error"_s));
    return;
  }

  if (!url_query.hasQueryItem(u"code"_s)) {
    Q_EMIT AuthenticationFinished(false, tr("Request URL is missing code!"));
    return;
  }

  if (!url_query.hasQueryItem(u"state"_s)) {
    Q_EMIT AuthenticationFinished(false, tr("Request URL is missing state!"));
    return;
  }

  if (url_query.queryItemValue(u"state"_s) != state_) {
    Q_EMIT AuthenticationFinished(false, tr("Request URL has wrong state %1 != %2").arg(url_query.queryItemValue(u"state"_s), state_));
    return;
  }

  RequestAccessToken(url_query.queryItemValue(u"code"_s), redirect_url);

}

QNetworkReply *OAuthenticator::CreateAccessTokenRequest(const ParamList &params, const bool refresh_token) {

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkRequest network_request(access_token_url_);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  if (type_ == Type::Client_Credentials && !client_id_.isEmpty() && !client_secret_.isEmpty()) {
    const QString authorization_header = client_id_ + u':' + client_secret_;
    network_request.setRawHeader("Authorization", "Basic " + authorization_header.toUtf8().toBase64());
  }

  QNetworkReply *reply = network_->post(network_request, url_query.toString(QUrl::FullyEncoded).toUtf8());
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &OAuthenticator::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, refresh_token]() { AccessTokenRequestFinished(reply, refresh_token); });

  return reply;

}

void OAuthenticator::RequestAccessToken(const QString &code, const QUrl &redirect_url) {

  if (timer_refresh_login_->isActive()) {
    timer_refresh_login_->stop();
  }

  ParamList params = ParamList() << Param(u"grant_type"_s, GrantType());

  if (!code.isEmpty()) {
    params << Param(u"code"_s, code);
  }

  if (!code_verifier_.isEmpty()) {
    params << Param(u"code_verifier"_s, code_verifier_);
  }

  if (!code.isEmpty()) {
    params << Param(u"redirect_uri"_s, redirect_url.toString());
  }

  if (!client_id_.isEmpty()) {
    params << Param(u"client_id"_s, client_id_);
  }

  if (!client_secret_.isEmpty()) {
    params << Param(u"client_secret"_s, client_secret_);
  }

  std::sort(params.begin(), params.end());

  CreateAccessTokenRequest(params, false);

}

void OAuthenticator::RerefreshAccessToken() {

  if (timer_refresh_login_->isActive()) {
    timer_refresh_login_->stop();
  }

  if (client_id_.isEmpty() || refresh_token_.isEmpty()) {
    return;
  }

  // Several requests rejected at the same time should only refresh the access token once.
  if (refresh_in_progress_) return;
  refresh_in_progress_ = true;

  ParamList params = ParamList() << Param(u"grant_type"_s, u"refresh_token"_s)
                                 << Param(u"client_id"_s, client_id_)
                                 << Param(u"refresh_token"_s, refresh_token_);

  if (!client_secret_.isEmpty()) {
    params << Param(u"client_secret"_s, client_secret_);
  }

  CreateAccessTokenRequest(params, true);

}

void OAuthenticator::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Debug) << settings_group_ << ssl_error.errorString();
  }

}

void OAuthenticator::AccessTokenRequestFinished(QNetworkReply *reply, const bool refresh_token) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (refresh_token) {
    refresh_in_progress_ = false;
  }

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    const QString error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    AccessTokenRequestFailed(refresh_token, error_message);
    return;
  }

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    const QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
      QJsonParseError json_error;
      const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_document.isEmpty() && json_document.isObject()) {
        const QJsonObject json_object = json_document.object();
        if (json_object.contains("error"_L1)) {
          const QString error = json_object["error"_L1].toString();
          const QString error_description = json_object["error_description"_L1].toString();
          // invalid_grant means the refresh token or authorization code is expired, revoked or otherwise unusable, so retrying with the same grant can never succeed.
          // When refreshing, the other OAuth errors also mean the same request will be rejected again, Tidal documents them as logging out.
          // Clear the session to stop the same failure from being repeated, the user has to authenticate again.
          const bool invalid_grant = error.compare("invalid_grant"_L1, Qt::CaseInsensitive) == 0;
          const bool clear_session = invalid_grant || (refresh_token && IsTerminalRefreshError(error));
          if (clear_session) {
            qLog(Error) << settings_group_ << "Authorization grant is no longer valid, clearing session.";
            ClearSession();
          }
          const QString error_message = error_description.isEmpty() ? error : QStringLiteral("%1 (%2)").arg(error, error_description);
          AccessTokenRequestFailed(refresh_token, error_message, clear_session);
          return;
        }
        qLog(Debug) << settings_group_ << "Unknown Json reply" << json_object;
      }
    }
    if (reply->error() == QNetworkReply::NoError) {
      AccessTokenRequestFailed(refresh_token, QStringLiteral("Received HTTP status code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    }
    else {
      AccessTokenRequestFailed(refresh_token, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    return;
  }

  const QByteArray data = reply->readAll();

  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    AccessTokenRequestFailed(refresh_token, QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_document.isEmpty()) {
    AccessTokenRequestFailed(refresh_token, u"Authentication reply from server has empty Json document."_s);
    return;
  }

  if (!json_document.isObject()) {
    AccessTokenRequestFailed(refresh_token, u"Authentication reply from server has Json document that is not an object."_s);
    return;
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    AccessTokenRequestFailed(refresh_token, u"Authentication reply from server has empty Json object."_s);
    return;
  }

  if (!json_object.contains("token_type"_L1)) {
    AccessTokenRequestFailed(refresh_token, u"Authentication reply from server is missing token type."_s);
    return;
  }

  if (!json_object.contains("access_token"_L1)) {
    AccessTokenRequestFailed(refresh_token, u"Authentication reply from server is missing access token."_s);
    return;
  }

  token_type_ = json_object["token_type"_L1].toString();
  access_token_ = json_object["access_token"_L1].toString();

  if (json_object.contains("refresh_token"_L1)) {
    refresh_token_ = json_object["refresh_token"_L1].toString();
  }
  else if (!refresh_token) {
    refresh_token_.clear();
  }

  if (json_object.contains("expires_in"_L1)) {
    expires_in_ = json_object["expires_in"_L1].toInt();
  }
  else {
    expires_in_ = 0;
  }

  login_time_ = QDateTime::currentSecsSinceEpoch();
  country_code_.clear();
  user_id_ = 0;
  refresh_retry_interval_ = 0;

  if (json_object.contains("user"_L1) && json_object["user"_L1].isObject()) {
    const QJsonObject object_user = json_object["user"_L1].toObject();
    if (object_user.contains("countryCode"_L1) && object_user.contains("userId"_L1)) {
      country_code_ = object_user["countryCode"_L1].toString();
      user_id_ = static_cast<quint64>(object_user["userId"_L1].toInt());
    }
  }

  Settings s;

  s.beginGroup(settings_group_);
  s.setValue(kTokenType, token_type_);
  s.setValue(kAccessToken, access_token_);
  s.setValue(kLoginTime, login_time_);

  if (refresh_token_.isEmpty()) {
    s.remove(kRefreshToken);
  }
  else {
    s.setValue(kRefreshToken, refresh_token_);
  }

  if (expires_in_ == 0) {
    s.remove(kExpiresIn);
  }
  else {
    s.setValue(kExpiresIn, expires_in_);
  }

  if (country_code_.isEmpty()) {
    s.remove(kCountryCode);
  }
  else {
    s.setValue(kCountryCode, country_code_);
  }

  if (user_id_ == 0) {
    s.remove(kUserId);
  }
  else {
    s.setValue(kUserId, user_id_);
  }

  s.endGroup();

  StartRefreshLoginTimer();

  qLog(Debug) << settings_group_ << "Authentication was successful, login expires in" << expires_in_;

  Q_EMIT AuthenticationFinished(true);

}

void OAuthenticator::AccessTokenRequestFailed(const bool refresh_token, const QString &error, const bool clear_session) {

  // When refreshing fails, keep the session and try again later, since failures like network errors are usually temporary.
  // Refreshing happens in the background, so don't report a failure that is retried, it would show an authentication failed message every time.
  if (refresh_token && !clear_session && !refresh_token_.isEmpty()) {
    qLog(Error) << settings_group_ << "Refreshing login failed:" << error;
    ScheduleRefreshRetry();
    return;
  }

  Q_EMIT AuthenticationFinished(false, error, clear_session);

}
