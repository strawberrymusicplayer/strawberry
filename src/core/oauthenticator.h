/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OAUTHENTICATOR_H
#define OAUTHENTICATOR_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSslError>

#include "includes/shared_ptr.h"

class QTimer;
class QNetworkReply;
class NetworkAccessManager;
class LocalRedirectServer;

class OAuthenticator : public QObject {
  Q_OBJECT

 public:
  explicit OAuthenticator(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~OAuthenticator() override;

  enum class Type {
    Authorization_Code,
    Client_Credentials
  };

  void set_settings_group(const QString &settings_group);
  void set_type(const Type type);
  void set_authorize_url(const QUrl &auth_url);
  void set_redirect_url(const QUrl &redirect_url);
  void set_access_token_url(const QUrl &access_token_url);
  void set_client_id(const QString &client_id);
  void set_client_secret(const QString &client_secret);
  void set_scope(const QString &scope);
  void set_use_local_redirect_server(const bool use_local_redirect_server);
  void set_random_port(const bool random_port);

  QString token_type() const { return token_type_; }
  QString access_token() const { return access_token_; }
  qint64 expires_in() const { return expires_in_; }
  QString country_code() const { return country_code_; }
  quint64 user_id() const { return user_id_; }
  bool authenticated() const { return !token_type_.isEmpty() && !access_token_.isEmpty(); }

  QByteArray authorization_header() const;

  void Authenticate();
  void ClearSession();
  void LoadSession();
  void ExternalAuthorizationUrlReceived(const QUrl &request_url);

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  QString GrantType() const;
  void StartRefreshLoginTimer();
  QNetworkReply *CreateAccessTokenRequest(const ParamList &params, const bool refresh_token);
  void RequestAccessToken(const QString &code = QString(), const QUrl &redirect_url = QUrl());
  void RerefreshAccessToken();
  void AuthorizationUrlReceived(const QUrl &request_url, const QUrl &redirect_url);

 Q_SIGNALS:
  void Error(const QString &error);
  void AuthenticationFinished(const bool success, const QString &error = QString());

 private Q_SLOTS:
  void RedirectArrived();
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void AccessTokenRequestFinished(QNetworkReply *reply, const bool refresh_token);

 private:
  const SharedPtr<NetworkAccessManager> network_;
  QScopedPointer<LocalRedirectServer, QScopedPointerDeleteLater> local_redirect_server_;
  QTimer *timer_refresh_login_;

  QString settings_group_;
  Type type_;
  QUrl authorize_url_;
  QUrl redirect_url_;
  QUrl access_token_url_;
  QString client_id_;
  QString client_secret_;
  QString scope_;
  bool use_local_redirect_server_;
  bool random_port_;

  QString code_verifier_;
  QString code_challenge_;

  QString token_type_;
  QString access_token_;
  QString refresh_token_;
  qint64 expires_in_;
  qint64 login_time_;
  QString country_code_;
  quint64 user_id_;

  QList<QNetworkReply*> replies_;
};

#endif  // OAUTHENTICATOR_H
