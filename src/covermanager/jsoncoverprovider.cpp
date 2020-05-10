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

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>

#include "core/application.h"
#include "coverprovider.h"
#include "jsoncoverprovider.h"

JsonCoverProvider::JsonCoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool fetchall, const bool allow_missing_album, Application *app, QObject *parent) : CoverProvider(name, enabled, authentication_required, quality, fetchall, allow_missing_album, app, parent) {}

QJsonObject JsonCoverProvider::ExtractJsonObj(const QByteArray &data) {

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
