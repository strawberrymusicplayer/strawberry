/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QObject>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QIODevice>
#include <QMutex>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>
#include <QAbstractNetworkCache>

#include "network.h"

QMutex ThreadSafeNetworkDiskCache::sMutex;
ThreadSafeNetworkDiskCache *ThreadSafeNetworkDiskCache::sInstance = nullptr;
QNetworkDiskCache *ThreadSafeNetworkDiskCache::sCache = nullptr;

ThreadSafeNetworkDiskCache::ThreadSafeNetworkDiskCache(QObject *parent)
: QAbstractNetworkCache(parent) {

  QMutexLocker l(&sMutex);
  if (!sCache) {
    sInstance = this;
    sCache = new QNetworkDiskCache;
#ifdef Q_OS_WIN32
    sCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/strawberry/networkcache");
#else
    sCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/networkcache");
#endif
  }

}

ThreadSafeNetworkDiskCache::~ThreadSafeNetworkDiskCache() {
  if (this == sInstance) delete sCache;
}

qint64 ThreadSafeNetworkDiskCache::cacheSize() const {
  QMutexLocker l(&sMutex);
  return sCache->cacheSize();
}

QIODevice *ThreadSafeNetworkDiskCache::data(const QUrl &url) {
  QMutexLocker l(&sMutex);
  return sCache->data(url);
}

void ThreadSafeNetworkDiskCache::insert(QIODevice *device) {
  QMutexLocker l(&sMutex);
  sCache->insert(device);
}

QNetworkCacheMetaData ThreadSafeNetworkDiskCache::metaData(const QUrl &url) {
  QMutexLocker l(&sMutex);
  return sCache->metaData(url);
}

QIODevice *ThreadSafeNetworkDiskCache::prepare(const QNetworkCacheMetaData &metaData) {
  QMutexLocker l(&sMutex);
  return sCache->prepare(metaData);
}

bool ThreadSafeNetworkDiskCache::remove(const QUrl &url) {
  QMutexLocker l(&sMutex);
  return sCache->remove(url);
}

void ThreadSafeNetworkDiskCache::updateMetaData(const QNetworkCacheMetaData &metaData) {
  QMutexLocker l(&sMutex);
  sCache->updateMetaData(metaData);
}

void ThreadSafeNetworkDiskCache::clear() {
  QMutexLocker l(&sMutex);
  sCache->clear();
}

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent) {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
  setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

  setCache(new ThreadSafeNetworkDiskCache(this));

}

QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) {

  QByteArray user_agent;
  if (request.hasRawHeader("User-Agent")) {
    user_agent = request.rawHeader("User-Agent");
  }
  else {
    user_agent = QString("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()).toUtf8();
  }

  QNetworkRequest new_request(request);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  new_request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

  new_request.setRawHeader("User-Agent", user_agent);

  if (op == QNetworkAccessManager::PostOperation && !new_request.header(QNetworkRequest::ContentTypeHeader).isValid()) {
    new_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  }

  // Prefer the cache unless the caller has changed the setting already
  if (request.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt() == QNetworkRequest::PreferNetwork) {
    new_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
  }

  return QNetworkAccessManager::createRequest(op, new_request, outgoingData);
}
