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

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "qobuzservice.h"

class QNetworkReply;
class NetworkAccessManager;

class QobuzBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit QobuzBaseRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~QobuzBaseRequest();

  enum class Type {
    None,
    FavouriteArtists,
    FavouriteAlbums,
    FavouriteSongs,
    SearchArtists,
    SearchAlbums,
    SearchSongs,
    StreamURL
  };

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  QNetworkReply *CreateRequest(const QString &ressource_name, const ParamList &params_provided);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(QByteArray &data);
  QJsonValue ExtractItems(QByteArray &data);
  QJsonValue ExtractItems(QJsonObject &json_obj);

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;
  static QString ErrorsToHTML(const QStringList &errors);

  QString app_id() const { return service_->app_id(); }
  QString app_secret() const { return service_->app_secret(); }
  QString username() const { return service_->username(); }
  QString password() const { return service_->password(); }
  int format() const { return service_->format(); }
  int artistssearchlimit() const { return service_->artistssearchlimit(); }
  int albumssearchlimit() const { return service_->albumssearchlimit(); }
  int songssearchlimit() const { return service_->songssearchlimit(); }

  qint64 user_id() const { return service_->user_id(); }
  QString user_auth_token() const { return service_->user_auth_token(); }
  QString device_id() const { return service_->device_id(); }
  qint64 credential_id() const { return service_->credential_id(); }

  bool authenticated() const { return service_->authenticated(); }
  bool login_sent() const { return service_->login_sent(); }
  int max_login_attempts() const { return service_->max_login_attempts(); }
  int login_attempts() const { return service_->login_attempts(); }

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 private:
  QobuzService *service_;
  const SharedPtr<NetworkAccessManager> network_;
};

#endif  // QOBUZBASEREQUEST_H
