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
#include <QString>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

#include "lyricsprovider.h"
#include "jsonlyricsprovider.h"

JsonLyricsProvider::JsonLyricsProvider(const QString &name, const bool enabled, const bool authentication_required, QObject *parent) : LyricsProvider(name, enabled, authentication_required, parent) {}

QByteArray JsonLyricsProvider::ExtractData(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    if (reply->error() < 200) {
      return QByteArray();
    }
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
  }

  return reply->readAll();

}

QJsonObject JsonLyricsProvider::ExtractJsonObj(const QByteArray &data) {

  if (data.isEmpty()) {
    return QJsonObject();
  }

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QString("Failed to parse json data: %1").arg(json_error.errorString()));
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
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

QJsonObject JsonLyricsProvider::ExtractJsonObj(QNetworkReply *reply) {

  return ExtractJsonObj(ExtractData(reply));

}
