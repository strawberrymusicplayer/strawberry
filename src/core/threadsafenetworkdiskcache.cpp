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
#include <QIODevice>
#include <QMutex>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>
#include <QAbstractNetworkCache>
#include <QUrl>
#include <QList>
#include <QHash>

#include "standardpaths.h"
#include "threadsafenetworkdiskcache.h"
#include "threadsafenetworkdiskcachedevice.h"

using namespace Qt::Literals::StringLiterals;

QMutex ThreadSafeNetworkDiskCache::sMutex;
int ThreadSafeNetworkDiskCache::sInstances = 0;
QNetworkDiskCache *ThreadSafeNetworkDiskCache::sCache = nullptr;
QHash<QUrl, ThreadSafeNetworkDiskCache::Saving> ThreadSafeNetworkDiskCache::sInserting;
QList<ThreadSafeNetworkDiskCache::Saving> ThreadSafeNetworkDiskCache::sCancelled;

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

  // The replies of the network access manager are deleted before its cache.
  CancelUnusedSavingsLocked(QList<QUrl>());

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

  // The reply is done with the device, like with QNetworkDiskCache, the device is deleted.
  for (QHash<QUrl, Saving>::iterator it = sInserting.begin(); it != sInserting.end(); ++it) {
    if (it.value().device == device) {
      ThreadSafeNetworkDiskCacheDevice *saving_device = it.value().device;
      const QUrl url = it.key();
      sInserting.erase(it);
      if (saving_device->cache_device()) {
        sCache->insert(saving_device->cache_device());
      }
      else {
        // Before Qt 6.6, QNetworkDiskCache deletes the device when expiring the cache, the saving is still in QNetworkDiskCache, it's removed so that it isn't removed instead of the cached data or the next saving of the URL.
        sCache->remove(url);
      }
      delete saving_device;
      return;
    }
  }

  // The saving was cancelled, the data isn't cached.
  for (QList<Saving>::iterator it = sCancelled.begin(); it != sCancelled.end(); ++it) {
    if (it->device == device) {
      delete it->device;
      sCancelled.erase(it);
      return;
    }
  }

}

QNetworkCacheMetaData ThreadSafeNetworkDiskCache::metaData(const QUrl &url) {
  QMutexLocker l(&sMutex);
  return sCache->metaData(url);
}

QIODevice *ThreadSafeNetworkDiskCache::prepare(const QNetworkCacheMetaData &metaData) {

  QMutexLocker l(&sMutex);

  prepare_failed_url_.clear();

  // QNetworkDiskCache cancels the first saving of the URL it finds with remove(), so only one reply at a time saves a URL, the cache is shared by all the network access managers, so this is for all of them.
  if (!IsInserting(metaData.url())) {
    QIODevice *cache_device = sCache->prepare(metaData);
    if (cache_device) {
      ThreadSafeNetworkDiskCacheDevice *device = new ThreadSafeNetworkDiskCacheDevice(cache_device, &sMutex);
      sInserting.insert(metaData.url(), Saving{metaData.url(), device, this});
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
  // The saving without a device is still in QNetworkDiskCache, it's removed so that it isn't removed instead of the next saving of the URL, or of the cached data.
  if (!it.value().device->cache_device()) {
    CancelSaving(url);
    return false;
  }

  return true;

}

void ThreadSafeNetworkDiskCache::CancelSaving(const QUrl &url) {

  // The reply can still be writing to the device, it's kept until the reply is done.
  sCancelled << sInserting.take(url);
  sCache->remove(url);

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

  // The reply saving the URL cancels its saving with remove(), but so do the other replies with the URL, a POST, PUT or DELETE invalidating it, and a reply which failed, which can't be told apart.
  // So the saving is cancelled, the data can be from before the invalidation, but the device is kept for the reply saving the URL.
  bool removed = false;
  if (IsInserting(cache_url)) {
    CancelSaving(cache_url);
    removed = true;
  }

  // The cached data is removed too, QNetworkDiskCache only removes the saving when there is one.
  return sCache->remove(cache_url) || removed;

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
  // The replies don't tell the cache when they are done with a cancelled saving either.
  // A saving is used by a reply of the network access manager with the URL, so when none of the replies which can still use their saving has the URL, the saving isn't used anymore.
  for (QHash<QUrl, Saving>::iterator it = sInserting.begin(); it != sInserting.end();) {
    if (it.value().cache == this && !reply_urls.contains(it.key())) {
      sCache->remove(it.key());
      delete it.value().device;
      it = sInserting.erase(it);
    }
    else {
      ++it;
    }
  }

  for (QList<Saving>::iterator it = sCancelled.begin(); it != sCancelled.end();) {
    if (it->cache == this && !reply_urls.contains(it->url)) {
      delete it->device;
      it = sCancelled.erase(it);
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
