/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QString>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"

using namespace Qt::Literals::StringLiterals;

SpotifyBaseRequest::SpotifyBaseRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service) {}

QString SpotifyBaseRequest::service_name() const {

  return service_->name();

}

bool SpotifyBaseRequest::authentication_required() const {

  return true;

}

bool SpotifyBaseRequest::authenticated() const {

  return service_->authenticated();

}

bool SpotifyBaseRequest::use_authorization_header() const {

  return true;

}

QByteArray SpotifyBaseRequest::authorization_header() const {

  return service_->authorization_header();

}

int SpotifyBaseRequest::artistssearchlimit() const {

  return service_->artistssearchlimit();

}

int SpotifyBaseRequest::albumssearchlimit() const {

  return service_->albumssearchlimit();

}

int SpotifyBaseRequest::songssearchlimit() const {

  return service_->songssearchlimit();

}

QNetworkReply *SpotifyBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  return CreateGetRequest(QUrl(QLatin1String(SpotifyService::kApiUrl) + QLatin1Char('/') + ressource_name), params_provided);

}

QJsonValue SpotifyBaseRequest::ExtractItems(const QJsonObject &json_object) {

  return GetJsonValue(json_object, u"items"_s);

}
