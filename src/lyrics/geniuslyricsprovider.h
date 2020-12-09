/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GENIUSLYRICSPROVIDER_H
#define GENIUSLYRICSPROVIDER_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSslError>
#include <QJsonArray>

#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"

class QNetworkReply;
class NetworkAccessManager;
class LocalRedirectServer;

class GeniusLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit GeniusLyricsProvider(QObject *parent = nullptr);
  ~GeniusLyricsProvider() override;

  bool IsAuthenticated() const override { return !access_token_.isEmpty(); }
  void Authenticate() override;
  void Deauthenticate() override { access_token_.clear(); }

  bool StartSearch(const QString &artist, const QString &album, const QString &title, quint64 id) override;
  void CancelSearch(const quint64 id) override;

 public:
  struct GeniusLyricsLyricContext {
    explicit GeniusLyricsLyricContext() {}
    QString artist;
    QString title;
    QUrl url;
  };
  struct GeniusLyricsSearchContext {
    explicit GeniusLyricsSearchContext() : id(-1) {}
    int id;
    QString artist;
    QString title;
    QMap<QUrl, GeniusLyricsLyricContext> requests_lyric_;
    LyricsSearchResults results;
  };

 private:
  void RequestAccessToken(const QUrl &url, const QUrl &redirect_url);
  void AuthError(const QString &error = QString(), const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  void EndSearch(std::shared_ptr<GeniusLyricsSearchContext> search, const GeniusLyricsLyricContext lyric = GeniusLyricsLyricContext());

 private slots:
  void HandleLoginSSLErrors(QList<QSslError> ssl_errors);
  void RedirectArrived();
  void AccessTokenRequestFinished(QNetworkReply *reply);
  void HandleSearchReply(QNetworkReply *reply, const quint64 id);
  void HandleLyricReply(QNetworkReply *reply, const int search_id, const QUrl &url);

 private:
  static const char *kSettingsGroup;
  static const char *kClientIDB64;
  static const char *kClientSecretB64;
  static const char *kOAuthAuthorizeUrl;
  static const char *kOAuthAccessTokenUrl;
  static const char *kOAuthRedirectUrl;
  static const char *kUrlSearch;

 private:
  NetworkAccessManager *network_;
  LocalRedirectServer *server_;
  QString code_verifier_;
  QString code_challenge_;
  QString access_token_;
  QStringList login_errors_;
  QMap<int, std::shared_ptr<GeniusLyricsSearchContext>> requests_search_;
  QList<QNetworkReply*> replies_;

};

#endif  // GENIUSLYRICSPROVIDER_H
