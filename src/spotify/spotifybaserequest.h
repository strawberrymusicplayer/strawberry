/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYBASEREQUEST_H
#define SPOTIFYBASEREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QSet>
#include <QList>
#include <QPair>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSslError>
#include <QJsonObject>
#include <QJsonValue>

#include "spotifyservice.h"

class QNetworkReply;
class NetworkAccessManager;

class SpotifyBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit SpotifyBaseRequest(SpotifyService *service, NetworkAccessManager *network, QObject *parent = nullptr);

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

 protected:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  QNetworkReply *CreateRequest(const QString &ressource_name, const ParamList &params_provided);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  QJsonValue ExtractItems(const QByteArray &data);
  QJsonValue ExtractItems(const QJsonObject &json_obj);

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;
  static QString ErrorsToHTML(const QStringList &errors);

  int artistssearchlimit() { return service_->artistssearchlimit(); }
  int albumssearchlimit() { return service_->albumssearchlimit(); }
  int songssearchlimit() { return service_->songssearchlimit(); }

  QString access_token() { return service_->access_token(); }

  bool authenticated() { return service_->authenticated(); }

 private slots:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  SpotifyService *service_;
  NetworkAccessManager *network_;

};

#endif  // SPOTIFYBASEREQUEST_H
