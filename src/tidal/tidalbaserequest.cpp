/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QByteArray>
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"

using namespace Qt::Literals::StringLiterals;

TidalBaseRequest::TidalBaseRequest(TidalService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service),
      network_(network) {}

QString TidalBaseRequest::service_name() const {

  return service_->name();

}

QByteArray TidalBaseRequest::AuthorizationHeader() const {

  return service_->AuthorizationHeader();

}

QNetworkReply *TidalBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  const ParamList params = ParamList() << params_provided
                                       << Param(u"countryCode"_s, country_code());
  return CreateGetRequest(QUrl(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + ressource_name), params);

}

QJsonValue TidalBaseRequest::ExtractItems(const QJsonObject &json_object) {

  return GetJsonValue(json_object, u"items"_s);

}
