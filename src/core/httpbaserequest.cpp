/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/logging.h"
#include "networkaccessmanager.h"
#include "httpbaserequest.h"

using namespace Qt::Literals::StringLiterals;

HttpBaseRequest::HttpBaseRequest(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network) {}

HttpBaseRequest::~HttpBaseRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QNetworkReply *HttpBaseRequest::CreateGetRequest(const QUrl &resource_url, const ParamList &params_provided) {

  QUrl request_url(resource_url);

  if (!params_provided.isEmpty()) {
    ParamList params = params_provided;
    std::sort(params.begin(), params.end());
    QUrlQuery url_query;
    for (const Param &param : params) {
      url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
    }
    request_url.setQuery(url_query);
  }

  QNetworkRequest http_request(request_url);
  http_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  http_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  if (use_authorization_header() && authenticated()) {
    http_request.setRawHeader("Authorization", AuthorizationHeader());
  }
  QNetworkReply *reply = network_->get(http_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &HttpBaseRequest::HandleSSLErrors);

  qLog(Debug) << service_name() << "Sending get request" << request_url;

  return reply;

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QByteArray &content_type_header, const QByteArray &data) {

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setHeader(QNetworkRequest::ContentTypeHeader, content_type_header);
  if (use_authorization_header() && authenticated()) {
    request.setRawHeader("Authorization", AuthorizationHeader());
  }
  QNetworkReply *reply = network_->post(request, data);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &HttpBaseRequest::HandleSSLErrors);

  qLog(Debug) << service_name() << "Sending post request" << url;

  return reply;

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QUrlQuery &url_query) {

  return CreatePostRequest(url, "application/x-www-form-urlencoded", url_query.toString(QUrl::FullyEncoded).toUtf8());

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const ParamList &params) {

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  return CreatePostRequest(url, url_query);

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QJsonDocument &json_document) {

  return CreatePostRequest(url, "application/json; charset=utf-8", json_document.toJson());

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QJsonObject &json_object) {

  return CreatePostRequest(url, QJsonDocument(json_object));

}

void HttpBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

HttpBaseRequest::ReplyDataResult HttpBaseRequest::GetReplyData(QNetworkReply *reply, const bool call_error) {

  int http_status_code = 200;
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }
  if (reply->error() == QNetworkReply::NoError && http_status_code == 200) {
    return reply->readAll();
  }

  ReplyDataResult result(ErrorCode::NetworkError);
  result.network_error = reply->error();
  result.http_status_code = http_status_code;

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    // This is a network error, there is nothing more to do.
    const QString error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    if (call_error) {
      Error(error_message);
    }
    return result;
  }

  // See if there is Json data containing a API error.
  const QByteArray data = reply->readAll();
  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error == QJsonParseError::NoError && !json_document.isEmpty() && json_document.isObject()) {
    QJsonObject json_object = json_document.object();
    qLog(Debug) << json_object;
    if (json_object.contains("error"_L1) && json_object["error"_L1].isObject()) {
      const QJsonObject object_error = json_object["error"_L1].toObject();
      // Spotify, Dropbox
      if (object_error.contains("status"_L1) && object_error.contains("message"_L1)) {
        const int status = object_error["status"_L1].toInt();
        const QString message = object_error["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.api_error = status;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(status);
      }
      else if (object_error.contains("code"_L1) && object_error.contains("message"_L1)) {
        const int code = object_error["code"_L1].toInt();
        const QString message = object_error["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.api_error = code;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
      }
      else {
        Error(u"Unknown Json error reponse"_s, object_error);
      }
    }
    // Listenbrainz
    if (json_object.contains("error"_L1) && json_object.contains("error_description"_L1)) {
      const int error = json_object["error"_L1].toInt();
      const QString error_description = json_object["error_description"_L1].toString();
      result.error_code = ErrorCode::APIError;
      result.api_error = error;
      result.error_message = QStringLiteral("%1 (%2)").arg(error_description).arg(error);
    }
    // Qobuz
    else if (json_object.contains("status"_L1) && json_object.contains("code"_L1) && json_object.contains("message"_L1)) {
      const int code = json_object["code"_L1].toInt();
      const QString message = json_object["message"_L1].toString();
      result.error_code = ErrorCode::APIError;
      result.api_error = code;
      result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
    }
    // Tidal
    else if (json_object.contains("status"_L1) && json_object.contains("subStatus"_L1) && json_object.contains("userMessage"_L1)) {
      const int status = json_object["status"_L1].toInt();
      const int sub_status = json_object["subStatus"_L1].toInt();
      const QString user_message = json_object["userMessage"_L1].toString();
      result.error_code = ErrorCode::APIError;
      result.error_message = QStringLiteral("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
    }
    // ListenBrainz
    else if (json_object.contains("code"_L1) && json_object.contains("error"_L1)) {
      const int code = json_object["code"_L1].toInt();
      const QString error = json_object["error"_L1].toString();
      result.error_code = ErrorCode::APIError;
      result.api_error = code;
      result.error_message = QStringLiteral("%1 (%2) (%3)").arg(code).arg(error);
    }
    // Last.fm
    else if (json_object.contains("error"_L1) && json_object.contains("message"_L1)) {
      const int error = json_object["error"_L1].toInt();
      const QString message = json_object["message"_L1].toString();
      result.error_code = ErrorCode::APIError;
      result.api_error = error;
      result.error_message = QStringLiteral("%1 (%2)").arg(error).arg(message);
    }
    else {
      Error(u"Unknown Json error reponse"_s, json_object);
    }
  }
  if (result.error_message.isEmpty()) {
    if (reply->error() == QNetworkReply::NoError) {
      result.error_message = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    }
    else {
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
  }

  if (call_error) {
    Error(result.error_message);
  }

  return result;

}

void HttpBaseRequest::Error(const QString &error_message, const QVariant &debug_output) {

  error_ = QStringLiteral("%1: %2").arg(service_name(), error_message);

  qLog(Error) << error_;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

}
