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

#ifndef SUBSONICBASEREQUEST_H
#define SUBSONICBASEREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/song.h"
#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "internet/internetsearch.h"
#include "subsonicservice.h"

class Application;
class NetworkAccessManager;
class SubsonicUrlHandler;
class CollectionBackend;
class CollectionModel;

class SubsonicBaseRequest : public QObject {
  Q_OBJECT

 public:

  SubsonicBaseRequest(SubsonicService *service, NetworkAccessManager *network, QObject *parent);
  ~SubsonicBaseRequest();

  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<EncodedParam> EncodedParamList;

  QUrl CreateUrl(const QString &ressource_name, const QList<Param> &params_provided);
  QNetworkReply *CreateGetRequest(const QString &ressource_name, const QList<Param> &params_provided);
  QByteArray GetReplyData(QNetworkReply *reply, QString &error);
  QJsonObject ExtractJsonObj(QByteArray &data, QString &error);

  virtual QString Error(QString error, QVariant debug = QVariant());

  QString client_name() { return service_->client_name(); }
  QString api_version() { return service_->api_version(); }
  QUrl server_url() { return service_->server_url(); }
  QString username() { return service_->username(); }
  QString password() { return service_->password(); }
  bool verify_certificate() { return service_->verify_certificate(); }
  bool download_album_covers() { return service_->download_album_covers(); }

 private:

  SubsonicService *service_;
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // SUBSONICBASEREQUEST_H
