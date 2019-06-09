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

#ifndef TIDALBASEREQUEST_H
#define TIDALBASEREQUEST_H

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
#include "tidalservice.h"

class Application;
class NetworkAccessManager;
class TidalUrlHandler;
class CollectionBackend;
class CollectionModel;

class TidalBaseRequest : public QObject {
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

  TidalBaseRequest(TidalService *service, NetworkAccessManager *network, QObject *parent);
  ~TidalBaseRequest();

  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<EncodedParam> EncodedParamList;

  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<Param> &params_provided);
  QByteArray GetReplyData(QNetworkReply *reply, QString &error, const bool send_login);
  QJsonObject ExtractJsonObj(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QJsonObject &json_obj, QString &error);

  virtual QString Error(QString error, QVariant debug = QVariant());

  QString api_url() { return QString(kApiUrl); }
  const bool oauth() { return service_->oauth(); }
  QString client_id() { return service_->client_id(); }
  QString api_token() { return service_->api_token(); }
  quint64 user_id() { return service_->user_id(); }
  QString country_code() { return service_->country_code(); }
  QString username() { return service_->username(); }
  QString password() { return service_->password(); }
  QString quality() { return service_->quality(); }
  int artistssearchlimit() { return service_->artistssearchlimit(); }
  int albumssearchlimit() { return service_->albumssearchlimit(); }
  int songssearchlimit() { return service_->songssearchlimit(); }
  bool fetchalbums() { return service_->fetchalbums(); }
  QString coversize() { return service_->coversize(); }

  QString access_token() { return service_->access_token(); }
  QString session_id() { return service_->session_id(); }

  bool authenticated() { return service_->authenticated(); }
  bool need_login() { return need_login(); }
  bool login_sent() { return service_->login_sent(); }
  int max_login_attempts() { return service_->max_login_attempts(); }
  int login_attempts() { return service_->login_attempts(); }

  virtual void NeedLogin() = 0;

 private:

  static const char *kApiUrl;

  TidalService *service_;
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // TIDALBASEREQUEST_H
