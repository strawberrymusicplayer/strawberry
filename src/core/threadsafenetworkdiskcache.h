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

#ifndef THREADSAFENETWORKDISKCACHE_H
#define THREADSAFENETWORKDISKCACHE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractNetworkCache>
#include <QMutex>
#include <QUrl>
#include <QList>
#include <QHash>
#include <QPointer>
#include <QNetworkCacheMetaData>

class QIODevice;
class QNetworkDiskCache;

class ThreadSafeNetworkDiskCache : public QAbstractNetworkCache {
  Q_OBJECT

 public:
  explicit ThreadSafeNetworkDiskCache(QObject *parent);
  ~ThreadSafeNetworkDiskCache() override;

  qint64 cacheSize() const override;
  QIODevice *data(const QUrl &url) override;
  void insert(QIODevice *device) override;
  QNetworkCacheMetaData metaData(const QUrl &url) override;
  QIODevice *prepare(const QNetworkCacheMetaData &metaData) override;
  bool remove(const QUrl &url) override;
  void updateMetaData(const QNetworkCacheMetaData &metaData) override;

  // The URL the cache uses for the URL of a reply, QNetworkCacheMetaData removes the password and the fragment.
  static QUrl CacheUrl(const QUrl &url);

  // Cancels the savings of the replies of the network access manager using this cache, for the URLs none of the replies in reply_urls has.
  // It's called when a reply is deleted or has finished with an error, with the URLs of the replies which can still use their saving.
  void CancelUnusedSavings(const QList<QUrl> &reply_urls);

 public Q_SLOTS:
  void clear() override;

 private:
  struct Saving {
    QPointer<QIODevice> device;
    // The cache of the network access manager with the reply saving the URL.
    ThreadSafeNetworkDiskCache *cache;
  };

  // Called with sMutex locked.
  static bool IsInserting(const QUrl &url);
  void CancelUnusedSavingsLocked(const QList<QUrl> &reply_urls);

  static QMutex sMutex;
  static int sInstances;
  static QNetworkDiskCache *sCache;
  // The URLs being saved, with the devices they are saved with.
  static QHash<QUrl, Saving> sInserting;
  QUrl prepare_failed_url_;
};

#endif  // THREADSAFENETWORKDISKCACHE_H
