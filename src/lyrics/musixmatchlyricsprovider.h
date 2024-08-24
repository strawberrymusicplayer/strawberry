/*
 * Strawberry Music Player
 * Copyright 2020-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "providers/musixmatchprovider.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchLyricsProvider : public JsonLyricsProvider, public MusixmatchProvider {
  Q_OBJECT

 public:
  explicit MusixmatchLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~MusixmatchLyricsProvider() override;

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
  bool CreateLyricsRequest(LyricsSearchContextPtr search);
  void SendLyricsRequest(const LyricsSearchRequest &request, const QString &artist, const QString &title);
  bool SendLyricsRequest(LyricsSearchContextPtr search, const QUrl &url);
  void EndSearch(LyricsSearchContextPtr search, const QUrl &url = QUrl());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search);
  void HandleLyricsReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search, const QUrl &url);

 private:
  QList<LyricsSearchContextPtr> requests_search_;
  QList<QNetworkReply*> replies_;
  bool use_api_;
};

#endif  // MUSIXMATCHLYRICSPROVIDER_H
