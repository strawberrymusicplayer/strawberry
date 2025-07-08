/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QMap>
#include <QString>
#include <QUrl>
#include <QMutex>

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;
class OAuthenticator;

class GeniusLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit GeniusLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void Authenticate() override;
  void ClearSession() override;

  virtual bool authenticated() const override;
  virtual bool use_authorization_header() const override;
  virtual QByteArray authorization_header() const override;

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
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void EndSearch(GeniusLyricsSearchContextPtr search, const GeniusLyricsLyricContext &lyric = GeniusLyricsLyricContext());
  void EndSearch(const int id, const LyricsSearchRequest &request, const LyricsSearchResults &results = LyricsSearchResults());

 private Q_SLOTS:
  void OAuthFinished(const bool success, const QString &error);
  void HandleSearchReply(QNetworkReply *reply, const int id);
  void HandleLyricReply(QNetworkReply *reply, const int search_id, const QUrl &url);

 private:
  static bool StartsOrEndsMatch(QString s, QString t);

 private:
  OAuthenticator *oauth_;
  mutable QMutex mutex_access_token_;
  QMap<int, SharedPtr<GeniusLyricsSearchContext>> requests_search_;
};

#endif  // GENIUSLYRICSPROVIDER_H
