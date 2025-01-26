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

#ifndef DROPBOXBASEREQUEST_H
#define DROPBOXBASEREQUEST_H

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class DropboxService;

class DropboxBaseRequest : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit DropboxBaseRequest(const SharedPtr<NetworkAccessManager> network, DropboxService *service, QObject *parent = nullptr);

  QString service_name() const override;
  bool authentication_required() const override;
  bool authenticated() const override;
  bool use_authorization_header() const override;
  QByteArray authorization_header() const override;

 protected:
  QNetworkReply *GetTemporaryLink(const QUrl &url);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 Q_SIGNALS:
  void ShowErrorDialog(const QString &error);

 private:
  DropboxService *service_;
};

#endif  // DROPBOXBASEREQUEST_H
