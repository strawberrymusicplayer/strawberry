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

#include <utility>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "utilities/randutils.h"
#include "subsonicservice.h"
#include "subsonicbaserequest.h"

#include "constants/subsonicsettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

SubsonicBaseRequest::SubsonicBaseRequest(SubsonicService *service, QObject *parent)
    : QObject(parent),
      service_(service),
      network_(new QNetworkAccessManager) {

  network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

}

QUrl SubsonicBaseRequest::CreateUrl(const QUrl &server_url, const SubsonicSettings::AuthMethod auth_method, const QString &username, const QString &password, const QString &ressource_name, const ParamList &params_provided) {

  ParamList params = ParamList() << params_provided
                                 << Param(u"c"_s, QLatin1String(SubsonicService::kClientName))
                                 << Param(u"v"_s, QLatin1String(SubsonicService::kApiVersion))
                                 << Param(u"f"_s, u"json"_s)
                                 << Param(u"u"_s, username);

  if (auth_method == SubsonicSettings::AuthMethod::Hex) {
    params << Param(u"p"_s, u"enc:"_s + QString::fromUtf8(password.toUtf8().toHex()));
  }
  else {
    const QString salt = Utilities::CryptographicRandomString(20);
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(password.toUtf8());
    md5.addData(salt.toUtf8());
    params << Param(u"s"_s, salt);
    params << Param(u"t"_s, QString::fromUtf8(md5.result().toHex()));
  }

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(server_url);

  if (!url.path().isEmpty() && url.path().right(1) == u'/') {
    url.setPath(url.path() + "rest/"_L1 + ressource_name + ".view"_L1);
  }
  else {
    url.setPath(url.path() + "/rest/"_L1 + ressource_name + ".view"_L1);
  }

  url.setQuery(url_query);

  return url;

}

QNetworkReply *SubsonicBaseRequest::CreateGetRequest(const QString &ressource_name, const ParamList &params_provided) const {

  const QUrl url = CreateUrl(server_url(), auth_method(), username(), password(), ressource_name, params_provided);
  QNetworkRequest network_request(url);

  if (url.scheme() == "https"_L1 && !verify_certificate()) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2());

  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SubsonicBaseRequest::HandleSSLErrors);

  //qLog(Debug) << "Subsonic: Sending request" << url;

  return reply;

}

void SubsonicBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

JsonBaseRequest::JsonObjectResult SubsonicBaseRequest::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
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
      if (!json_object.isEmpty() && json_object.contains("error"_L1) && json_object["error"_L1].isObject()) {
        const QJsonObject object_error = json_object["error"_L1].toObject();
        if (!object_error.isEmpty() && object_error.contains("code"_L1) && object_error.contains("message"_L1)) {
          const int code = object_error["code"_L1].toInt();
          const QString message = object_error["message"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%s (%s)").arg(message, code);
        }
      }
      else {
        if (json_object.contains("subsonic-response"_L1) && json_object["subsonic-response"_L1].isObject()) {
          result.json_object = json_object["subsonic-response"_L1].toObject();
        }
        else {
          result.json_object = json_object;
        }
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
    //service_->ClearSession();
  }

  return result;

}
