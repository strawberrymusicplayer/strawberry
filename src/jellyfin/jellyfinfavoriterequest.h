/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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


#ifndef JELLYFINFAVORITEREQUEST_H
#define JELLYFINFAVORITEREQUEST_H

#include "config.h"

#include <QList>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "jellyfinbaserequest.h"
#include "jellyfinservice.h"

class QNetworkReply;
class NetworkAccessManager;

// Adds and removes favorite artists, albums and songs, which the catalogs show.
class JellyfinFavoriteRequest : public JellyfinBaseRequest {
  Q_OBJECT

 public:
  explicit JellyfinFavoriteRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void SendChanges(const JellyfinFavoriteChangeList &changes);

  // Abort the requests, for example when the server changed.
  void Clear();

 Q_SIGNALS:
  // All requests for the catalog finished, the error is empty when they succeeded.
  void FavoritesChanged(const int catalog, const QString &error);
  // The server rejected the access token, the changes can be sent again after logging in.
  void FavoritesUnauthorized(const QString &access_token, const JellyfinFavoriteChangeList &changes);

 private:
  void SendChange(const JellyfinFavoriteChange &change);
  void ReplyReceived(QNetworkReply *reply, const JellyfinFavoriteChange &change);

  QHash<int, int> requests_active_;
  QHash<int, QStringList> errors_;
  QSet<QString> items_in_flight_;
  JellyfinFavoriteChangeList waiting_changes_;  // Changes of an item with a change in flight, sent in order when it finished.
  QString unauthorized_access_token_;
  JellyfinFavoriteChangeList unauthorized_changes_;
};

#endif  // JELLYFINFAVORITEREQUEST_H
