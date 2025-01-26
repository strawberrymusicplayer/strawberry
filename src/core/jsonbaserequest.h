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

#ifndef JSONBASEREQUEST_H
#define JSONBASEREQUEST_H

#include "config.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "httpbaserequest.h"

class NetworkAccessManager;

class JsonBaseRequest : public HttpBaseRequest {
  Q_OBJECT

 public:
  explicit JsonBaseRequest(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

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

#endif  // JSONBASEREQUEST_H
