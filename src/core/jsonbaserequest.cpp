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

#include <QByteArray>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include "networkaccessmanager.h"
#include "jsonbaserequest.h"

using namespace Qt::Literals::StringLiterals;

JsonBaseRequest::JsonBaseRequest(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HttpBaseRequest(network, parent) {}

JsonBaseRequest::JsonObjectResult JsonBaseRequest::GetJsonObject(const QByteArray &data) {

  if (data.isEmpty()) {
    return JsonObjectResult(ErrorCode::ParseError, "Empty data from server"_L1);
  }

  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    return JsonObjectResult(ErrorCode::ParseError, json_error.errorString());
  }

  if (json_document.isEmpty()) {
    return JsonObjectResult(ErrorCode::ParseError, "Received empty Json document."_L1);
  }

  if (!json_document.isObject()) {
    return JsonObjectResult(ErrorCode::ParseError, "Json document is not an object."_L1);
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    return JsonObjectResult(ErrorCode::ParseError, "Received empty Json object."_L1);
  }

  return json_object;

}

JsonBaseRequest::JsonValueResult JsonBaseRequest::GetJsonValue(const QJsonObject &json_object, const QString &name) {

  if (!json_object.contains(name)) {
    return JsonValueResult(ErrorCode::ParseError, QStringLiteral("Json object is missing value %1.").arg(name));
  }

  return json_object[name];

}

JsonBaseRequest::JsonObjectResult JsonBaseRequest::GetJsonObject(const QJsonObject &json_object, const QString &name) {

  if (!json_object.contains(name)) {
    return JsonValueResult(ErrorCode::ParseError, QStringLiteral("Json object is missing object %1.").arg(name));
  }

  const QJsonValue json_value = json_object[name];
  if (!json_value.isObject()) {
    return JsonValueResult(ErrorCode::ParseError, QStringLiteral("Json value %1 is not a object.").arg(name));
  }

  return json_value.toObject();

}

JsonBaseRequest::JsonArrayResult JsonBaseRequest::GetJsonArray(const QJsonObject &json_object, const QString &name) {

  const JsonValueResult json_value_result = GetJsonValue(json_object, name);
  if (!json_value_result.success()) {
    return JsonArrayResult(ErrorCode::ParseError, json_value_result.error_message);
  }

  if (!json_value_result.json_value.isArray()) {
    return JsonArrayResult(ErrorCode::ParseError, QStringLiteral("Json object value %1 is not a array.").arg(name));
  }

  return json_value_result.json_value.toArray();

}
