/*
 * Strawberry Music Player
 * Copyright 2022-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYBASEREQUEST_H
#define SPOTIFYBASEREQUEST_H

#include "config.h"

#include <QString>
#include <QJsonObject>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"

#include "spotifyservice.h"

class QNetworkReply;
class NetworkAccessManager;

class SpotifyBaseRequest : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit SpotifyBaseRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  enum class Type {
    None,
    FavouriteArtists,
    FavouriteAlbums,
    FavouriteSongs,
    SearchArtists,
    SearchAlbums,
    SearchSongs,
    StreamURL,
  };

 protected:
  QString service_name() const override { return service_->name();  }
  bool authenticated() const override { return service_->authenticated(); }
  QString access_token() const { return service_->access_token(); }
  bool use_authorization_header() const override { return true; }
  QByteArray AuthorizationHeader() const override { return "Bearer " + access_token().toUtf8(); }
  int artistssearchlimit() const { return service_->artistssearchlimit(); }
  int albumssearchlimit() const { return service_->albumssearchlimit(); }
  int songssearchlimit() const { return service_->songssearchlimit(); }

  QNetworkReply *CreateRequest(const QString &ressource_name, const ParamList &params_provided);
  QJsonValue ExtractItems(const QJsonObject &json_object);

 private:
  SpotifyService *service_;
};

#endif  // SPOTIFYBASEREQUEST_H
