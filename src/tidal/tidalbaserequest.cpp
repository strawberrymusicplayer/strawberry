/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QJsonValue>
#include <QJsonObject>

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

bool TidalBaseRequest::authentication_required() const {

  return true;

}

bool TidalBaseRequest::authenticated() const {

  return service_->authenticated();

}

bool TidalBaseRequest::use_authorization_header() const {

  return true;

}

QByteArray TidalBaseRequest::authorization_header() const {

  return service_->authorization_header();

}

QString TidalBaseRequest::client_id() const {

  return service_->client_id();

}

QString TidalBaseRequest::quality() const {

  return service_->quality();

}

int TidalBaseRequest::artistssearchlimit() const {

  return service_->artistssearchlimit();

}

int TidalBaseRequest::albumssearchlimit() const {

  return service_->albumssearchlimit();

}

int TidalBaseRequest::songssearchlimit() const {

  return service_->songssearchlimit();

}

quint64 TidalBaseRequest::user_id() const {

  return service_->user_id();

}

QString TidalBaseRequest::country_code() const {

  return service_->country_code();

}

QNetworkReply *TidalBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  const ParamList params = ParamList() << params_provided
                                       << Param(u"countryCode"_s, country_code());
  return CreateGetRequest(QUrl(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + ressource_name), params);

}

QJsonValue TidalBaseRequest::ExtractItems(const QJsonObject &json_object) {

  return GetJsonValue(json_object, u"items"_s);

}
