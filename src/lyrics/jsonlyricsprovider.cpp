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

#include <QByteArray>
#include <QString>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "jsonlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

JsonLyricsProvider::JsonLyricsProvider(const QString &name, const bool enabled, const bool authentication_required, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : LyricsProvider(name, enabled, authentication_required, network, parent) {}

JsonLyricsProvider::JsonObjectResult JsonLyricsProvider::GetJsonObject(const QByteArray &data) {

  QJsonParseError json_error;
  const QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    const QString error = json_error.errorString();
    Error(error, data);
    return JsonObjectResult(ErrorCode::ParseError, error);
  }

  if (json_doc.isEmpty()) {
    const QString error = "Received empty Json document."_L1;
    Error(error, data);
    return JsonObjectResult(ErrorCode::ParseError, error);
  }

  if (!json_doc.isObject()) {
    const QString error = "Json document is not an object."_L1;
    Error(error, json_doc);
    return JsonObjectResult(ErrorCode::ParseError, error);
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    const QString error = "Received empty Json object."_L1;
    Error(error, json_doc);
    return JsonObjectResult(ErrorCode::ParseError, error);
  }

  return json_obj;

}

JsonLyricsProvider::JsonObjectResult JsonLyricsProvider::GetJsonObject(QNetworkReply *reply) {

  return GetJsonObject(reply->readAll());

}

QJsonValue JsonLyricsProvider::GetJsonValue(const QJsonObject &json_object, const QString &name) {

  if (!json_object.contains(name)) {
    Error(QStringLiteral("Json object is missing %1.").arg(name), json_object);
    return QJsonArray();
  }

  return json_object[name];

}

QJsonArray JsonLyricsProvider::GetJsonArray(const QJsonObject &json_object, const QString &name) {

  const QJsonValue json_value = GetJsonValue(json_object, name);
  if (!json_value.isArray()) {
    Error(QStringLiteral("%1 is not an array.").arg(name), json_object);
    return QJsonArray();
  }

  return json_value.toArray();

}
