/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QHash>
#include <QPointer>

#include "standardpaths.h"
#include "threadsafenetworkdiskcache.h"

using namespace Qt::Literals::StringLiterals;

QMutex ThreadSafeNetworkDiskCache::sMutex;
int ThreadSafeNetworkDiskCache::sInstances = 0;
QNetworkDiskCache *ThreadSafeNetworkDiskCache::sCache = nullptr;
QHash<QUrl, ThreadSafeNetworkDiskCache::Saving> ThreadSafeNetworkDiskCache::sInserting;

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
    sInserting.clear();
  }
  else {
    // The replies of the network access manager are deleted before its cache.
    CancelUnusedSavingsLocked(QList<QUrl>());
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

  for (QHash<QUrl, Saving>::iterator it = sInserting.begin(); it != sInserting.end(); ++it) {
    if (it.value().device == device) {
      sInserting.erase(it);
      break;
    }
  }

  sCache->insert(device);

}

QNetworkCacheMetaData ThreadSafeNetworkDiskCache::metaData(const QUrl &url) {
  QMutexLocker l(&sMutex);
  return sCache->metaData(url);
}

QIODevice *ThreadSafeNetworkDiskCache::prepare(const QNetworkCacheMetaData &metaData) {

  QMutexLocker l(&sMutex);

  prepare_failed_url_.clear();

  // The replies cancel the saving with remove(), which cancels the first saving of the URL, also when it's another reply's, and deletes its device while that reply is still writing to it.
  // So only one reply at a time saves a URL, the cache is shared by all the network access managers, so this is for all of them.
  if (!IsInserting(metaData.url())) {
    QIODevice *device = sCache->prepare(metaData);
    if (device) {
      sInserting.insert(metaData.url(), Saving{device, this});
      return device;
    }
  }

  // A reply which doesn't get a device calls remove() with the URL right away, which must not cancel another reply's saving.
  prepare_failed_url_ = metaData.url();

  return nullptr;

}

bool ThreadSafeNetworkDiskCache::IsInserting(const QUrl &url) {

  const QHash<QUrl, Saving>::iterator it = sInserting.find(url);
  if (it == sInserting.end()) return false;

  // Before Qt 6.6, QNetworkDiskCache deletes the device of a saving when expiring the cache, without the reply calling insert() or remove(), so the URL isn't being saved anymore.
  // The saving without a device is still there, it's removed so that it isn't removed instead of the next saving of the URL.
  if (it.value().device.isNull()) {
    sInserting.erase(it);
    sCache->remove(url);
    return false;
  }

  return true;

}

QUrl ThreadSafeNetworkDiskCache::CacheUrl(const QUrl &url) {

  QNetworkCacheMetaData metadata;
  metadata.setUrl(url);
  return metadata.url();

}

bool ThreadSafeNetworkDiskCache::remove(const QUrl &url) {

  // The replies remove their URL, the savings have the URL from QNetworkCacheMetaData, without the password and the fragment.
  const QUrl cache_url = CacheUrl(url);

  QMutexLocker l(&sMutex);

  if (!prepare_failed_url_.isEmpty() && prepare_failed_url_ == cache_url) {
    prepare_failed_url_.clear();
    if (IsInserting(cache_url)) return false;
    return sCache->remove(cache_url);
  }

  // A reply deleted while saving the URL cancels the saving after its saving was cancelled by CancelUnusedSavings(), when the URL can be saved by another network access manager's reply already.
  const QHash<QUrl, Saving>::const_iterator it = sInserting.constFind(cache_url);
  if (it != sInserting.constEnd() && it.value().cache != this && !it.value().device.isNull()) {
    return false;
  }

  // Otherwise it's the reply saving the URL cancelling it, or the URL isn't being saved.
  sInserting.remove(cache_url);

  return sCache->remove(cache_url);

}

void ThreadSafeNetworkDiskCache::updateMetaData(const QNetworkCacheMetaData &metaData) {
  QMutexLocker l(&sMutex);
  sCache->updateMetaData(metaData);
}

void ThreadSafeNetworkDiskCache::CancelUnusedSavings(const QList<QUrl> &reply_urls) {

  QMutexLocker l(&sMutex);
  CancelUnusedSavingsLocked(reply_urls);

}

void ThreadSafeNetworkDiskCache::CancelUnusedSavingsLocked(const QList<QUrl> &reply_urls) {

  // Before Qt 6.5.10, 6.8.5 and 6.9.2, a reply deleted while saving a URL doesn't cancel the saving, so the URL wouldn't be saved again.
  // Only one reply of a network access manager saves a URL, so when none of the replies which can still use their saving has the URL, the saving isn't used anymore.
  for (QHash<QUrl, Saving>::iterator it = sInserting.begin(); it != sInserting.end();) {
    if (it.value().cache == this && !reply_urls.contains(it.key())) {
      // The saving is the only saving of the URL, so it's the saving which is removed.
      sCache->remove(it.key());
      it = sInserting.erase(it);
    }
    else {
      ++it;
    }
  }

}

void ThreadSafeNetworkDiskCache::clear() {
  QMutexLocker l(&sMutex);
  sCache->clear();
}
