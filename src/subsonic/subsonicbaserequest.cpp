/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

  QUrl url = CreateUrl(server_url(), auth_method(), username(), password(), ressource_name, params_provided);
  QNetworkRequest req(url);

  if (url.scheme() == "https"_L1 && !verify_certificate()) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(sslconfig);
  }

  req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2());

  QNetworkReply *reply = network_->get(req);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SubsonicBaseRequest::HandleSSLErrors);

  //qLog(Debug) << "Subsonic: Sending request" << url;

  return reply;

}

void SubsonicBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

QByteArray SubsonicBaseRequest::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {

      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1)) {
          QJsonValue json_error = json_obj["error"_L1];
          if (json_error.isObject()) {
            json_obj = json_error.toObject();
            if (!json_obj.isEmpty() && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
              int code = json_obj["code"_L1].toInt();
              QString message = json_obj["message"_L1].toString();
              error = QStringLiteral("%1 (%2)").arg(message).arg(code);
            }
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      Error(error);
    }
  }

  return data;

}

QJsonObject SubsonicBaseRequest::ExtractJsonObj(QByteArray &data) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(u"Reply from server missing Json data."_s, data);
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
    Error(u"Received empty Json document."_s, data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(u"Json document is not an object."_s, json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(u"Received empty Json object."_s, json_doc);
    return QJsonObject();
  }

  if (!json_obj.contains("subsonic-response"_L1)) {
    Error(u"Json reply is missing subsonic-response."_s, json_obj);
    return QJsonObject();
  }

  QJsonValue json_response = json_obj["subsonic-response"_L1];
  if (!json_response.isObject()) {
    Error(u"Json response is not an object."_s, json_response);
    return QJsonObject();
  }
  json_obj = json_response.toObject();

  return json_obj;

}

QString SubsonicBaseRequest::ErrorsToHTML(const QStringList &errors) {

  QString error_html;
  for (const QString &error : errors) {
    error_html += error + "<br />"_L1;
  }
  return error_html;

}
