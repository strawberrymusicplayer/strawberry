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
#include <QObject>
#include <QCoreApplication>
#include <QIODevice>
#include <QMutex>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>
#include <QAbstractNetworkCache>
#include <QUrl>

#include "standardpaths.h"
#include "threadsafenetworkdiskcache.h"

using namespace Qt::Literals::StringLiterals;

QMutex ThreadSafeNetworkDiskCache::sMutex;
int ThreadSafeNetworkDiskCache::sInstances = 0;
QNetworkDiskCache *ThreadSafeNetworkDiskCache::sCache = nullptr;

ThreadSafeNetworkDiskCache::ThreadSafeNetworkDiskCache(QObject *parent) : QAbstractNetworkCache(parent) {

  QMutexLocker l(&sMutex);
  ++sInstances;

  if (!sCache) {
    sCache = new QNetworkDiskCache;
#ifdef Q_OS_WIN32
    sCache->setCacheDirectory(StandardPaths::WritableLocation(StandardPaths::StandardLocation::TempLocation) + u"/strawberry/networkcache"_s);
#else
    sCache->setCacheDirectory(StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/networkcache"_s);
#endif
  }

}

ThreadSafeNetworkDiskCache::~ThreadSafeNetworkDiskCache() {

  QMutexLocker l(&sMutex);
  --sInstances;

  if (sCache && sInstances == 0) {
    sCache->deleteLater();
    sCache = nullptr;
  }

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
