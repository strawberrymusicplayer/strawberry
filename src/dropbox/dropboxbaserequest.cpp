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

#include "config.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

#include "constants/dropboxconstants.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "dropboxservice.h"
#include "dropboxbaserequest.h"

using namespace Qt::Literals::StringLiterals;
using namespace DropboxConstants;

DropboxBaseRequest::DropboxBaseRequest(const SharedPtr<NetworkAccessManager> network, DropboxService *service, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service) {}

QString DropboxBaseRequest::service_name() const {

  return service_->name();

}

bool DropboxBaseRequest::authenticated() const {

  return service_->authenticated();

}

bool DropboxBaseRequest::use_authorization_header() const {

  return true;

}

QByteArray DropboxBaseRequest::AuthorizationHeader() const {

  return service_->AuthorizationHeader();

}

QNetworkReply *DropboxBaseRequest::GetTemporaryLink(const QUrl &url) {

  QJsonObject json_object;
  json_object.insert("path"_L1, url.path());
  return CreatePostRequest(QUrl(QLatin1String(kApiUrl) + "/2/files/get_temporary_link"_L1), json_object);

}
