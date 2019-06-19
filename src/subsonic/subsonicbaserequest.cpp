/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QSslConfiguration>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/network.h"
#include "subsonicservice.h"
#include "subsonicbaserequest.h"

SubsonicBaseRequest::SubsonicBaseRequest(SubsonicService *service, NetworkAccessManager *network, QObject *parent) :
      QObject(parent),
      service_(service),
      network_(network)
      {}

SubsonicBaseRequest::~SubsonicBaseRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

QUrl SubsonicBaseRequest::CreateUrl(const QString &ressource_name, const QList<Param> &params_provided) {

  ParamList params = ParamList() << params_provided
                                 << Param("c", client_name())
                                 << Param("v", api_version())
                                 << Param("f", "json")
                                 << Param("u", username())
                                 << Param("p", QString("enc:" + password().toUtf8().toHex()));

  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(server_url());

  if (!url.path().isEmpty() && url.path().right(1) == "/") {
    url.setPath(url.path() + QString("rest/") + ressource_name);
  }
  else
    url.setPath(url.path() + QString("/rest/") + ressource_name);

  url.setQuery(url_query);

  return url;

}

QNetworkReply *SubsonicBaseRequest::CreateGetRequest(const QString &ressource_name, const QList<Param> &params_provided) {

  QUrl url = CreateUrl(ressource_name, params_provided);
  QNetworkRequest req(url);

  if (url.scheme() == "https" && !verify_certificate()) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(sslconfig);
  }

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;

  //qLog(Debug) << "Subsonic: Sending request" << url;

  return reply;

}

QByteArray SubsonicBaseRequest::GetReplyData(QNetworkReply *reply, QString &error) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    reply->deleteLater();
  }

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError) {
    int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http_code == 200) {
      data = reply->readAll();
    }
    else {
      error = Error(QString("Received HTTP code %1").arg(http_code));
    }
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      error = Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      QString failure_reason;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error")) {
          QJsonValue json_error = json_obj["error"];
          if (json_error.isObject()) {
            json_obj = json_error.toObject();
            if (!json_obj.isEmpty() && json_obj.contains("code") && json_obj.contains("message")) {
              int code = json_obj["code"].toInt();
              QString message = json_obj["message"].toString();
              failure_reason = QString("%1 (%2)").arg(message).arg(code);
            }
          }
        }
      }
      if (failure_reason.isEmpty()) {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      error = Error(failure_reason);
    }
  }

  return data;

}

QJsonObject SubsonicBaseRequest::ExtractJsonObj(QByteArray &data, QString &error) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    error = Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    error = Error("Received empty Json document.", data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    error = Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    error = Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  if (!json_obj.contains("subsonic-response")) {
    error = Error("Json reply is missing subsonic-response.", json_obj);
    return QJsonObject();
  }

  QJsonValue json_response = json_obj["subsonic-response"];
  if (!json_response.isObject()) {
    error = Error("Json response is not an object.", json_response);
    return QJsonObject();
  }
  json_obj = json_response.toObject();

  return json_obj;

}

QString SubsonicBaseRequest::Error(QString error, QVariant debug) {

  qLog(Error) << "Subsonic:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  return error;

}
