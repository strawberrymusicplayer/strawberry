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

QNetworkReply *SpotifyBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  return CreateGetRequest(QUrl(QLatin1String(SpotifyService::kApiUrl) + QLatin1Char('/') + ressource_name), params_provided);

}

QJsonValue SpotifyBaseRequest::ExtractItems(const QJsonObject &json_object) {

  return GetJsonValue(json_object, u"items"_s);

}
