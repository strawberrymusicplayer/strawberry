/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/networkaccessmanager.h"
#include "jellyfinservice.h"
#include "jellyfinbaserequest.h"

using namespace Qt::Literals::StringLiterals;

JellyfinBaseRequest::JellyfinBaseRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service),
      network_(network) {}

QString JellyfinBaseRequest::service_name() const {
  return QStringLiteral("Jellyfin");
}

bool JellyfinBaseRequest::authentication_required() const {
  return true;
}

bool JellyfinBaseRequest::authenticated() const {
  return service_->authenticated();
}

bool JellyfinBaseRequest::use_authorization_header() const {
  return false;
}

QByteArray JellyfinBaseRequest::authorization_header() const {
  return QByteArray();
}

QUrl JellyfinBaseRequest::server_url() const {
  return service_->server_url();
}

QString JellyfinBaseRequest::access_token() const {
  return service_->access_token();
}

bool JellyfinBaseRequest::http2() const {
  return service_->http2();
}

bool JellyfinBaseRequest::verify_certificate() const {
  return service_->verify_certificate();
}

bool JellyfinBaseRequest::download_album_covers() const {
  return service_->download_album_covers();
}

QUrl JellyfinBaseRequest::CreateUrl(const QString &ressource_path) const {

  QUrl url(service_->server_url());
  QString path = url.path();
  if (path.isEmpty()) path = u"/"_s;
  else if (!path.endsWith(u'/')) path.append(u'/');
  url.setPath(path + ressource_path);

  return url;

}

void JellyfinBaseRequest::SetRequestAttributes(QNetworkRequest &network_request) const {

  // Only follow redirects to the same server, since the Authorization and X-Emby-Token headers would be sent to the new location too.
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2());
  network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  network_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

  // Jellyfin versions 10.11+ only accept the access token when it is embedded in the standard Authorization header,
  // so always send it, in addition to the legacy X-Emby-Token header which older servers require.
  if (!access_token().isEmpty()) {
    network_request.setRawHeader("Authorization", service_->CreateAuthorizationHeader(true).toUtf8());
    network_request.setRawHeader("X-Emby-Token", access_token().toUtf8());
  }

  if (service_->server_url().scheme() == "https"_L1 && !verify_certificate()) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

}

QNetworkReply *JellyfinBaseRequest::CreateGetRequest(const QString &ressource_path, const ParamList &params) {

  const QUrl url = CreateUrl(ressource_path);

  QNetworkRequest network_request(url);
  SetRequestAttributes(network_request);

  if (!params.isEmpty()) {
    QUrlQuery url_query;
    for (const Param &param : std::as_const(params)) {
      url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
    }
    network_request.setUrl(QUrl(url.toString() + u'?' + url_query.toString(QUrl::FullyEncoded)));
  }

  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinBaseRequest::HandleSSLErrors);
  replies_ << reply;

  // qLog(Debug) << "Jellyfin: Sending request" << network_request.url();

  return reply;

}

QNetworkReply *JellyfinBaseRequest::CreateGetRequestFromUrl(const QUrl &url) {

  QNetworkRequest network_request(url);
  SetRequestAttributes(network_request);

  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinBaseRequest::HandleSSLErrors);
  replies_ << reply;

  // qLog(Debug) << "Jellyfin: Sending request" << url;

  return reply;

}

QNetworkReply *JellyfinBaseRequest::CreatePostRequest(const QString &ressource_path, const QJsonObject &json_object) {

  const QUrl url = CreateUrl(ressource_path);

  QNetworkRequest network_request(url);
  SetRequestAttributes(network_request);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json; charset=utf-8"_s);

  QNetworkReply *reply = network_->post(network_request, QJsonDocument(json_object).toJson(QJsonDocument::Compact));
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinBaseRequest::HandleSSLErrors);
  replies_ << reply;

  return reply;

}

JsonBaseRequest::JsonObjectResult JellyfinBaseRequest::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();

  if (result.http_status_code != 200) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError && !json_document.isEmpty() && json_document.isObject()) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains(u"Message"_s)) {
        result.error_code = ErrorCode::HttpError;
        result.error_message = QStringLiteral("%1 (%2)").arg(json_object.value(u"Message"_s).toString()).arg(result.http_status_code);
        return result;
      }
    }
    result.error_code = ErrorCode::HttpError;
    result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    return result;
  }

  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      if (!json_document.isEmpty() && json_document.isObject()) {
        result.json_object = json_document.object();
      }
      else {
        result.error_code = ErrorCode::ParseError;
        result.error_message = QStringLiteral("Unexpected Json response type.");
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::Success) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
  }

  return result;

}