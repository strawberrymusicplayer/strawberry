/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QMutex>

#include "core/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;
class LocalRedirectServer;

class GeniusLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit GeniusLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~GeniusLyricsProvider() override;

  bool IsAuthenticated() const override { return !access_token().isEmpty(); }
  void Authenticate() override;
  void Deauthenticate() override { clear_access_token(); }

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private:
  struct GeniusLyricsLyricContext {
    explicit GeniusLyricsLyricContext() {}
    QString artist;
    QString title;
    QUrl url;
  };
  struct GeniusLyricsSearchContext {
    explicit GeniusLyricsSearchContext() : id(-1) {}
    int id;
    LyricsSearchRequest request;
    QMap<QUrl, GeniusLyricsLyricContext> requests_lyric_;
    LyricsSearchResults results;
  };

  using GeniusLyricsSearchContextPtr = SharedPtr<GeniusLyricsSearchContext>;

 private:
  QString access_token() const;
  void clear_access_token();
  void set_access_token(const QString &access_token);
  void RequestAccessToken(const QUrl &url, const QUrl &redirect_url);
  void AuthError(const QString &error = QString(), const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  void EndSearch(GeniusLyricsSearchContextPtr search, const GeniusLyricsLyricContext &lyric = GeniusLyricsLyricContext());

 private Q_SLOTS:
  void HandleLoginSSLErrors(const QList<QSslError> &ssl_errors);
  void RedirectArrived();
  void AccessTokenRequestFinished(QNetworkReply *reply);
  void HandleSearchReply(QNetworkReply *reply, const int id);
  void HandleLyricReply(QNetworkReply *reply, const int search_id, const QUrl &url);

 private:
  LocalRedirectServer *server_;
  QString code_verifier_;
  QString code_challenge_;
  mutable QMutex mutex_access_token_;
  QString access_token_;
  QStringList login_errors_;
  QMap<int, SharedPtr<GeniusLyricsSearchContext>> requests_search_;
  QList<QNetworkReply*> replies_;
};

#endif  // GENIUSLYRICSPROVIDER_H
