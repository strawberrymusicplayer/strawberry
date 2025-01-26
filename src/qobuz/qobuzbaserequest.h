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
#include "core/jsonbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class QobuzService;

class QobuzBaseRequest : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit QobuzBaseRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

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
  QString service_name() const override;
  QString app_id() const;
  QString app_secret() const;
  QString username() const;
  QString password() const;
  int format() const;
  int artistssearchlimit() const;
  int albumssearchlimit() const;
  int songssearchlimit() const;

  qint64 user_id() const;
  QString user_auth_token() const;
  QString device_id() const;
  qint64 credential_id() const;

  bool authentication_required() const override;
  bool authenticated() const override;
  bool use_authorization_header() const override;
  QByteArray authorization_header() const override;

  bool login_sent() const;
  int max_login_attempts() const;
  int login_attempts() const;

  QNetworkReply *CreateRequest(const QString &ressource_name, const ParamList &params_provided);

 protected:
  QobuzService *service_;
  const SharedPtr<NetworkAccessManager> network_;
};

#endif  // QOBUZBASEREQUEST_H
