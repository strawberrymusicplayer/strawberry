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

#ifndef QOBUZBASEREQUEST_H
#define QOBUZBASEREQUEST_H

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
#include "qobuzservice.h"

class Application;
class NetworkAccessManager;
class QobuzUrlHandler;
class CollectionBackend;
class CollectionModel;

class QobuzBaseRequest : public QObject {
  Q_OBJECT

 public:

  enum QueryType {
    QueryType_None,
    QueryType_Artists,
    QueryType_Albums,
    QueryType_Songs,
    QueryType_SearchArtists,
    QueryType_SearchAlbums,
    QueryType_SearchSongs,
    QueryType_StreamURL,
  };

  QobuzBaseRequest(QobuzService *service, NetworkAccessManager *network, QObject *parent);
  ~QobuzBaseRequest();

  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<EncodedParam> EncodedParamList;

  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<Param> &params_provided);
  QByteArray GetReplyData(QNetworkReply *reply, QString &error);
  QJsonObject ExtractJsonObj(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QJsonObject &json_obj, QString &error);

  virtual QString Error(QString error, QVariant debug = QVariant());

  QString api_url() { return QString(kApiUrl); }
  QString app_id() { return service_->app_id(); }
  QString app_secret() { return service_->app_secret(); }
  QString username() { return service_->username(); }
  QString password() { return service_->password(); }
  int format() { return service_->format(); }
  int artistssearchlimit() { return service_->artistssearchlimit(); }
  int albumssearchlimit() { return service_->albumssearchlimit(); }
  int songssearchlimit() { return service_->songssearchlimit(); }

  QString user_auth_token() { return service_->user_auth_token(); }

  bool authenticated() { return service_->authenticated(); }
  bool login_sent() { return service_->login_sent(); }
  int max_login_attempts() { return service_->max_login_attempts(); }
  int login_attempts() { return service_->login_attempts(); }

 private:

  static const char *kApiUrl;

  QobuzService *service_;
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // QOBUZBASEREQUEST_H
