/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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

#ifndef PLEXBASEREQUEST_H
#define PLEXBASEREQUEST_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QPair>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSslError>
#include <QJsonObject>

#include "includes/scoped_ptr.h"
#include "core/jsonbaserequest.h"
#include "plexservice.h"

class QNetworkAccessManager;
class QNetworkReply;

class PlexBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit PlexBaseRequest(PlexService *service, QObject *parent = nullptr);

  using JsonObjectResult = JsonBaseRequest::JsonObjectResult;
  using ErrorCode = JsonBaseRequest::ErrorCode;

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  QUrl CreateUrl(const QString &ressource_path, const ParamList &params_provided) const;
  QNetworkReply *CreateGetRequest(const QString &ressource_path, const ParamList &params_provided) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

  QUrl server_url() const { return service_->server_url(); }
  QString token() const { return service_->server_token(); }
  QString client_id() const { return service_->client_id(); }
  bool verify_certificate() const { return service_->verify_certificate(); }
  bool download_album_covers() const { return service_->download_album_covers(); }

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  PlexService *service_;
  ScopedPtr<QNetworkAccessManager> network_;
};

#endif  // PLEXBASEREQUEST_H
