/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SUBSONICBASEREQUEST_H
#define SUBSONICBASEREQUEST_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QSet>
#include <QPair>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSslError>
#include <QJsonObject>

#include "includes/scoped_ptr.h"
#include "constants/subsonicsettings.h"
#include "core/jsonbaserequest.h"
#include "subsonicservice.h"

class QNetworkAccessManager;
class QNetworkReply;

class SubsonicBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit SubsonicBaseRequest(SubsonicService *service, QObject *parent = nullptr);

  using JsonObjectResult = JsonBaseRequest::JsonObjectResult;
  using ErrorCode = JsonBaseRequest::ErrorCode;

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

 public:
  static QUrl CreateUrl(const QUrl &server_url, const SubsonicSettings::AuthMethod auth_method, const QString &username, const QString &password, const QString &ressource_name, const ParamList &params_provided);

 protected:
  QNetworkReply *CreateGetRequest(const QString &ressource_name, const ParamList &params_provided) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

  QUrl server_url() const { return service_->server_url(); }
  QString username() const { return service_->username(); }
  QString password() const { return service_->password(); }
  SubsonicSettings::AuthMethod auth_method() const { return service_->auth_method(); }
  bool http2() const { return service_->http2(); }
  bool verify_certificate() const { return service_->verify_certificate(); }
  bool download_album_covers() const { return service_->download_album_covers(); }
  bool use_album_id_for_album_covers() const { return service_->use_album_id_for_album_covers(); }

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  SubsonicService *service_;
  ScopedPtr<QNetworkAccessManager> network_;
};

#endif  // SUBSONICBASEREQUEST_H
