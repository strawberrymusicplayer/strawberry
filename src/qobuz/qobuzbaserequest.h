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
#include <QPair>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonValue>

#include "core/song.h"
#include "qobuzservice.h"

class QNetworkReply;
class NetworkAccessManager;

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
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(QByteArray &data);
  QJsonValue ExtractItems(QByteArray &data);
  QJsonValue ExtractItems(QJsonObject &json_obj);

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;
  QString ErrorsToHTML(const QStringList &errors);

  QString api_url() { return QString(kApiUrl); }
  QString app_id() { return service_->app_id(); }
  QString app_secret() { return service_->app_secret(); }
  QString username() { return service_->username(); }
  QString password() { return service_->password(); }
  int format() { return service_->format(); }
  int artistssearchlimit() { return service_->artistssearchlimit(); }
  int albumssearchlimit() { return service_->albumssearchlimit(); }
  int songssearchlimit() { return service_->songssearchlimit(); }

  qint64 user_id() { return service_->user_id(); }
  QString user_auth_token() { return service_->user_auth_token(); }
  QString device_id() { return service_->device_id(); }
  qint64 credential_id() { return service_->credential_id(); }

  bool authenticated() { return service_->authenticated(); }
  bool login_sent() { return service_->login_sent(); }
  int max_login_attempts() { return service_->max_login_attempts(); }
  int login_attempts() { return service_->login_attempts(); }

 private slots:
  void HandleSSLErrors(QList<QSslError> ssl_errors);

 private:

  static const char *kApiUrl;

  QobuzService *service_;
  NetworkAccessManager *network_;

};

#endif  // QOBUZBASEREQUEST_H
