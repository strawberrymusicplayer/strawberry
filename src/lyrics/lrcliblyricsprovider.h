/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LRCLIBLYRICSPROVIDER_H
#define LRCLIBLYRICSPROVIDER_H

#include "config.h"

#include <QString>

#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;

class LrcLibLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit LrcLibLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 private:
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void EndSearch(const int id, const LyricsSearchRequest &request, const LyricsSearchResults &results);

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request);
};

#endif  // LRCLIBLYRICSPROVIDER_H
