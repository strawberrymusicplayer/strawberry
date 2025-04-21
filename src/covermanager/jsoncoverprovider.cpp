/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "coverprovider.h"
#include "jsoncoverprovider.h"

using namespace Qt::Literals::StringLiterals;

JsonCoverProvider::JsonCoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool batch, const bool allow_missing_album, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : CoverProvider(name, enabled, authentication_required, quality, batch, allow_missing_album, network, parent) {}

QJsonObject JsonCoverProvider::ExtractJsonObject(const QByteArray &data) {

  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    Error(QStringLiteral("Failed to parse Json data: %1").arg(json_error.errorString()));
    return QJsonObject();
  }

  if (json_document.isEmpty()) {
    Error(u"Received empty Json document."_s, data);
    return QJsonObject();
  }

  if (!json_document.isObject()) {
    Error(u"Json document is not an object."_s, json_document);
    return QJsonObject();
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    Error(u"Received empty Json object."_s, json_document);
    return QJsonObject();
  }

  return json_object;

}
