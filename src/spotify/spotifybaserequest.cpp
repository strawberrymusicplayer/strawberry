/*
 * Strawberry Music Player
 * Copyright 2022-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"

using namespace Qt::StringLiterals;

SpotifyBaseRequest::SpotifyBaseRequest(SpotifyService *service, NetworkAccessManager *network, QObject *parent)
    : QObject(parent),
      service_(service),
      network_(network) {}

QNetworkReply *SpotifyBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  QUrlQuery url_query;
  for (const Param &param : params_provided) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(SpotifyService::kApiUrl) + QLatin1Char('/') + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());

  QNetworkReply *reply = network_->get(req);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SpotifyBaseRequest::HandleSSLErrors);

  qLog(Debug) << "Spotify: Sending request" << url;

  return reply;

}

void SpotifyBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

QByteArray SpotifyBaseRequest::GetReplyData(QNetworkReply *reply) {

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
      // See if there is Json data containing "error".
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      int status = 0;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1) && json_obj["error"_L1].isObject()) {
          QJsonObject obj_error = json_obj["error"_L1].toObject();
          if (!obj_error.isEmpty() && obj_error.contains("status"_L1) && obj_error.contains("message"_L1)) {
            status = obj_error["status"_L1].toInt();
            QString user_message = obj_error["message"_L1].toString();
            error = QStringLiteral("%1 (%2)").arg(user_message).arg(status);
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() == QNetworkReply::NoError) {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
        else {
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject SpotifyBaseRequest::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QStringLiteral("Reply from server missing Json data."), data);
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
    Error(QStringLiteral("Received empty Json document."), data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(QStringLiteral("Json document is not an object."), json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(QStringLiteral("Received empty Json object."), json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue SpotifyBaseRequest::ExtractItems(const QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonValue();
  return ExtractItems(json_obj);

}

QJsonValue SpotifyBaseRequest::ExtractItems(const QJsonObject &json_obj) {

  if (!json_obj.contains("items"_L1)) {
    Error(QStringLiteral("Json reply is missing items."), json_obj);
    return QJsonArray();
  }
  QJsonValue json_items = json_obj["items"_L1];
  return json_items;

}

QString SpotifyBaseRequest::ErrorsToHTML(const QStringList &errors) {

  QString error_html;
  for (const QString &error : errors) {
    error_html += error + "<br />"_L1;
  }
  return error_html;

}
