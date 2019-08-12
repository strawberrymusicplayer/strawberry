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
#include "lyricsfetcher.h"
#include "jsonlyricsprovider.h"

JsonLyricsProvider::JsonLyricsProvider(const QString &name, QObject *parent) : LyricsProvider(name, parent) {}

QJsonObject JsonLyricsProvider::ExtractJsonObj(QNetworkReply *reply, const quint64 id) {

  QString failure_reason;
  if (reply->error() != QNetworkReply::NoError) {
    failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    if (reply->error() < 200) {
      Error(id, failure_reason);
      return QJsonObject();
    }
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    failure_reason = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    if (failure_reason.isEmpty()) failure_reason = "Empty reply received from server.";
    Error(id, failure_reason);
    return QJsonObject();
  }

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error(id, "Reply from server missing Json data.");
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error(id, "Received empty Json document.");
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(id, "Json document is not an object.");
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(id, "Received empty Json object.");
    return QJsonObject();
  }

  return json_obj;

}
