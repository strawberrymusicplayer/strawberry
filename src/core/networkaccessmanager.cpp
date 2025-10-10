/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QCoreApplication>
#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "networkaccessmanager.h"
#include "threadsafenetworkdiskcache.h"

using namespace Qt::Literals::StringLiterals;

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent) {

  setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  setCache(new ThreadSafeNetworkDiskCache(this));

}

QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) {

  QByteArray user_agent;
  if (network_request.hasRawHeader("User-Agent")) {
    user_agent = network_request.header(QNetworkRequest::UserAgentHeader).toByteArray();
  }
  else {
    user_agent = QStringLiteral("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()).toUtf8();
  }

  QNetworkRequest new_network_request(network_request);
  new_network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  new_network_request.setHeader(QNetworkRequest::UserAgentHeader, user_agent);

  if (op == QNetworkAccessManager::PostOperation && !new_network_request.header(QNetworkRequest::ContentTypeHeader).isValid()) {
    new_network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  }

  // Prefer the cache unless the caller has changed the setting already
  if (!network_request.attribute(QNetworkRequest::CacheLoadControlAttribute).isValid()) {
    new_network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
  }

  return QNetworkAccessManager::createRequest(op, new_network_request, outgoing_data);

}
