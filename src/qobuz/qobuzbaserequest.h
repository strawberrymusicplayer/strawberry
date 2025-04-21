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

#ifndef QOBUZBASEREQUEST_H
#define QOBUZBASEREQUEST_H

#include "config.h"

#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class QobuzService;

class QobuzBaseRequest : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit QobuzBaseRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  enum class Type {
    None,
    FavouriteArtists,
    FavouriteAlbums,
    FavouriteSongs,
    SearchArtists,
    SearchAlbums,
    SearchSongs,
    StreamURL
  };

 protected:
  QString service_name() const override;
  bool authentication_required() const override;
  bool authenticated() const override;
  bool use_authorization_header() const override;
  QByteArray authorization_header() const override;

  QNetworkReply *CreateRequest(const QString &ressource_name, const ParamList &params_provided);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 protected:
  QobuzService *service_;
  const SharedPtr<NetworkAccessManager> network_;
};

#endif  // QOBUZBASEREQUEST_H
