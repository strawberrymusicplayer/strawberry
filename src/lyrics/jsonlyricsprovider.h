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

#ifndef JSONLYRICSPROVIDER_H
#define JSONLYRICSPROVIDER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"
#include "lyricsprovider.h"

class NetworkAccessManager;
class QNetworkReply;

class JsonLyricsProvider : public LyricsProvider {
  Q_OBJECT

 public:
  explicit JsonLyricsProvider(const QString &name, const bool enabled, const bool authentication_required, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  class JsonObjectResult : public ReplyDataResult {
   public:
    JsonObjectResult(const ErrorCode _error_code, const QString &_error_message = QString()) : ReplyDataResult(_error_code, _error_message) {}
    JsonObjectResult(const QJsonObject &_json_object) : ReplyDataResult(ErrorCode::Success), json_object(_json_object) {}
    QJsonObject json_object;
  };

 protected:
  virtual JsonObjectResult GetJsonObject(const QByteArray &data);
  virtual JsonObjectResult GetJsonObject(QNetworkReply *reply);
  virtual QJsonValue GetJsonValue(const QJsonObject &json_object, const QString &name);
  virtual QJsonArray GetJsonArray(const QJsonObject &json_object, const QString &name);

};

#endif  // JSONLYRICSPROVIDER_H
