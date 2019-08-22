/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/network.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"

const char *TidalBaseRequest::kApiUrl = "https://api.tidalhifi.com/v1";

TidalBaseRequest::TidalBaseRequest(TidalService *service, NetworkAccessManager *network, QObject *parent) :
      QObject(parent),
      service_(service),
      network_(network)
      {}

TidalBaseRequest::~TidalBaseRequest() {}

QNetworkReply *TidalBaseRequest::CreateRequest(const QString &ressource_name, const QList<Param> &params_provided) {

  ParamList params = ParamList() << params_provided
                                 << Param("countryCode", country_code());

  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kApiUrl + QString("/") + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  if (!session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", session_id().toUtf8());

  QNetworkReply *reply = network_->get(req);
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleSSLErrors(QList<QSslError>)));

  //qLog(Debug) << "Tidal: Sending request" << url;

  return reply;

}

void TidalBaseRequest::HandleSSLErrors(QList<QSslError> ssl_errors) {

  for (QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

QByteArray TidalBaseRequest::GetReplyData(QNetworkReply *reply, const bool send_login) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "status" and "userMessage" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      int status = 0;
      int sub_status = 0;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          status = json_obj["status"].toInt();
          sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          error = QString("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (reply->error() != QNetworkReply::NoError) {
        error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      else {
        error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
      }
      if (status == 401 && sub_status == 6001) {  // User does not have a valid session
        emit service_->Logout();
        if (!oauth() && send_login && login_attempts() < max_login_attempts() && !api_token().isEmpty() && !username().isEmpty() && !password().isEmpty()) {
          qLog(Error) << "Tidal:" << error;
          qLog(Info) << "Tidal:" << "Attempting to login.";
          NeedLogin();
          emit service_->Login();
        }
        else {
          Error(error);
        }
      }
      else {
        Error(error);
      }
    }
    return QByteArray();
  }

  return data;

}

QJsonObject TidalBaseRequest::ExtractJsonObj(QByteArray &data) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
     Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue TidalBaseRequest::ExtractItems(QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonValue();
  return ExtractItems(json_obj);

}

QJsonValue TidalBaseRequest::ExtractItems(QJsonObject &json_obj) {

  if (!json_obj.contains("items")) {
    Error("Json reply is missing items.", json_obj);
    return QJsonArray();
  }
  QJsonValue json_items = json_obj["items"];
  return json_items;

}

QString TidalBaseRequest::ErrorsToHTML(const QStringList &errors) {

  QString error_html;
  for (const QString &error : errors) {
    error_html += error + "<br />";
  }
  return error_html;

}
