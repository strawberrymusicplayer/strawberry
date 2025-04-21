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

#ifndef MUSIXMATCHLYRICSPROVIDER_H
#define MUSIXMATCHLYRICSPROVIDER_H

#include "config.h"

#include <QList>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 private:
  struct LyricsSearchContext {
    explicit LyricsSearchContext() : id(-1) {}
    int id;
    LyricsSearchRequest request;
    QList<QUrl> requests_lyrics_;
    LyricsSearchResults results;
  };

  using LyricsSearchContextPtr = SharedPtr<LyricsSearchContext>;

  bool SendSearchRequest(LyricsSearchContextPtr search);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  bool CreateLyricsRequest(LyricsSearchContextPtr search);
  void SendLyricsRequest(const LyricsSearchRequest &request, const QString &artist, const QString &title);
  bool SendLyricsRequest(LyricsSearchContextPtr search, const QUrl &url);
  void EndSearch(LyricsSearchContextPtr search, const QUrl &url = QUrl());

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search);
  void HandleLyricsReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search, const QUrl &url);

 private:
  QList<LyricsSearchContextPtr> requests_search_;
  bool use_api_;
};

#endif  // MUSIXMATCHLYRICSPROVIDER_H
