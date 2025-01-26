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

#include "config.h"

#include <algorithm>
#include <utility>

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
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"

using namespace Qt::Literals::StringLiterals;

QobuzBaseRequest::QobuzBaseRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service),
      network_(network) {}

QString QobuzBaseRequest::service_name() const {

  return service_->name();

}

bool QobuzBaseRequest::authentication_required() const {

  return true;

}

bool QobuzBaseRequest::authenticated() const {

  return service_->authenticated();

}

bool QobuzBaseRequest::use_authorization_header() const {

  return false;

}

QByteArray QobuzBaseRequest::authorization_header() const {

  return QByteArray();

}

QNetworkReply *QobuzBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  ParamList params = ParamList() << params_provided
                                 << Param(u"app_id"_s, service_->app_id());

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(QobuzService::kApiUrl) + u'/' + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  network_request.setRawHeader("X-App-Id", service_->app_id().toUtf8());
  if (authenticated()) {
    network_request.setRawHeader("X-User-Auth-Token", service_->user_auth_token().toUtf8());
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &QobuzBaseRequest::HandleSSLErrors);

  qLog(Debug) << "Qobuz: Sending request" << url;

  return reply;

}

JsonBaseRequest::JsonObjectResult QobuzBaseRequest::ParseJsonObject(QNetworkReply *reply) {

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
      if (json_object.contains("code"_L1) && json_object.contains("status"_L1) && json_object.contains("message"_L1)) {
        const int code = json_object["code"_L1].toInt();
        const QString message = json_object["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
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

  if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
    service_->ClearSession();

  }

  return result;

}
