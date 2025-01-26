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

#include "config.h"

#include <algorithm>
#include <utility>

#include <QByteArray>
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"

using namespace Qt::Literals::StringLiterals;

QobuzBaseRequest::QobuzBaseRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      service_(service),
      network_(network) {}

QString QobuzBaseRequest::service_name() const { return service_->name();  }
QString QobuzBaseRequest::app_id() const { return service_->app_id(); }
QString QobuzBaseRequest::app_secret() const { return service_->app_secret(); }
QString QobuzBaseRequest::username() const { return service_->username(); }
QString QobuzBaseRequest::password() const { return service_->password(); }
int QobuzBaseRequest::format() const { return service_->format(); }
int QobuzBaseRequest::artistssearchlimit() const { return service_->artistssearchlimit(); }
int QobuzBaseRequest::albumssearchlimit() const { return service_->albumssearchlimit(); }
int QobuzBaseRequest::songssearchlimit() const { return service_->songssearchlimit(); }

qint64 QobuzBaseRequest::user_id() const { return service_->user_id(); }
QString QobuzBaseRequest::user_auth_token() const { return service_->user_auth_token(); }
QString QobuzBaseRequest::device_id() const { return service_->device_id(); }
qint64 QobuzBaseRequest::credential_id() const { return service_->credential_id(); }

bool QobuzBaseRequest::authentication_required() const { return true; }
bool QobuzBaseRequest::authenticated() const { return service_->authenticated(); }
bool QobuzBaseRequest::use_authorization_header() const { return false; }
QByteArray QobuzBaseRequest::authorization_header() const { return QByteArray(); }

bool QobuzBaseRequest::login_sent() const { return service_->login_sent(); }
int QobuzBaseRequest::max_login_attempts() const { return service_->max_login_attempts(); }
int QobuzBaseRequest::login_attempts() const { return service_->login_attempts(); }

QNetworkReply *QobuzBaseRequest::CreateRequest(const QString &ressource_name, const ParamList &params_provided) {

  ParamList params = ParamList() << params_provided
                                 << Param(u"app_id"_s, app_id());

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(QobuzService::kApiUrl) + QLatin1Char('/') + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  network_request.setRawHeader("X-App-Id", app_id().toUtf8());
  if (authenticated()) {
    network_request.setRawHeader("X-User-Auth-Token", user_auth_token().toUtf8());
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &QobuzBaseRequest::HandleSSLErrors);

  qLog(Debug) << "Qobuz: Sending request" << url;

  return reply;

}

QJsonValue QobuzBaseRequest::ExtractItems(const QJsonObject &json_object) {

  return GetJsonValue(json_object, u"items"_s);

}
