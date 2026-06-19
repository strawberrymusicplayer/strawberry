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
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "opentidalservice.h"
#include "opentidalbaserequest.h"

using namespace Qt::Literals::StringLiterals;

OpenTidalBaseRequest::OpenTidalBaseRequest(OpenTidalService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service),
      network_(network) {}

QString OpenTidalBaseRequest::service_name() const {

  return service_->name();

}

bool OpenTidalBaseRequest::authentication_required() const {

  return true;

}

bool OpenTidalBaseRequest::authenticated() const {

  return service_->authenticated();

}

bool OpenTidalBaseRequest::use_authorization_header() const {

  return true;

}

QByteArray OpenTidalBaseRequest::authorization_header() const {

  return service_->authorization_header();

}

QNetworkReply *OpenTidalBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  const ParamList params = ParamList() << params_provided
                                       << Param(u"countryCode"_s, service_->country_code());
  return CreateGetRequest(QUrl(QLatin1String(OpenTidalService::kApiUrl) + QLatin1Char('/') + ressource_name), params);

}

JsonBaseRequest::JsonObjectResult OpenTidalBaseRequest::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  bool clear_session = false;
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      // The TIDAL Open API returns errors in a JSON:API "errors" array.
      if (json_object.contains("errors"_L1) && json_object["errors"_L1].isArray()) {
        const QJsonArray array_errors = json_object["errors"_L1].toArray();
        for (const QJsonValue &value_error : array_errors) {
          if (!value_error.isObject()) continue;
          const QJsonObject object_error = value_error.toObject();
          const QString category = object_error["category"_L1].toString();
          const QString code = object_error["code"_L1].toString();
          const QString detail = object_error["detail"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%1 (%2) (%3)").arg(category, code, detail);
          if (category == "AUTHENTICATION_ERROR"_L1) {
            clear_session = true;
          }
        }
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
    else if (result.http_status_code < 200 || result.http_status_code >= 300) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  // Clear the session on auth failure so the user is prompted to log in again (matches Qobuz/Spotify).
  if (reply->error() == QNetworkReply::AuthenticationRequiredError || clear_session) {
    service_->ClearSession();
  }

  return result;

}
