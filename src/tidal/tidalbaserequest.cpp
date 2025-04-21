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
#include "core/logging.h"
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

QNetworkReply *TidalBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  const ParamList params = ParamList() << params_provided
                                       << Param(u"countryCode"_s, service_->country_code());
  return CreateGetRequest(QUrl(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + ressource_name), params);

}

JsonBaseRequest::JsonObjectResult TidalBaseRequest::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("status"_L1) && json_object.contains("subStatus"_L1) && json_object.contains("userMessage"_L1)) {
        const int status = json_object["status"_L1].toInt();
        const int sub_status = json_object["subStatus"_L1].toInt();
        const QString user_message = json_object["userMessage"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.api_error = status;
        result.error_message = QStringLiteral("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}
