/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OVHLYRICSPROVIDER_H
#define OVHLYRICSPROVIDER_H

#include "config.h"

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"

class QNetworkReply;
class NetworkAccessManager;

class OVHLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit OVHLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

private:
  OVHLyricsProvider::JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request);
};

#endif  // OVHLYRICSPROVIDER_H
