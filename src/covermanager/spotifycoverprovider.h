/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYCOVERPROVIDER_H
#define SPOTIFYCOVERPROVIDER_H

#include "config.h"

#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QSslError>
#include <QJsonValue>
#include <QJsonObject>
#include <QTimer>

#include "jsoncoverprovider.h"

class QNetworkReply;
class Application;
class NetworkAccessManager;
class LocalRedirectServer;

class SpotifyCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit SpotifyCoverProvider(Application *app, QObject *parent = nullptr);
  ~SpotifyCoverProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

  void Authenticate() override;
  void Deauthenticate() override;
  bool IsAuthenticated() const override { return !access_token_.isEmpty(); }

 private slots:
  void HandleLoginSSLErrors(QList<QSslError> ssl_errors);
  void RedirectArrived();
  void RequestAccessToken(const QString code = QString(), const QUrl redirect_url = QUrl());
  void AccessTokenRequestFinished(QNetworkReply *reply);
  void HandleSearchReply(QNetworkReply *reply, const int id, const QString &extract);

 private:
  QByteArray GetReplyData(QNetworkReply *reply);
  void AuthError(const QString &error = QString(), const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  static const char *kSettingsGroup;
  static const char *kClientIDB64;
  static const char *kClientSecretB64;
  static const char *kOAuthAuthorizeUrl;
  static const char *kOAuthAccessTokenUrl;
  static const char *kOAuthRedirectUrl;
  static const char *kApiUrl;
  static const int kLimit;

  NetworkAccessManager *network_;
  LocalRedirectServer *server_;
  QStringList login_errors_;
  QString code_verifier_;
  QString code_challenge_;
  QString access_token_;
  QString refresh_token_;
  quint64 expires_in_;
  quint64 login_time_;
  QTimer refresh_login_timer_;
  QList<QNetworkReply*> replies_;

};

#endif  // SPOTIFYCOVERPROVIDER_H
