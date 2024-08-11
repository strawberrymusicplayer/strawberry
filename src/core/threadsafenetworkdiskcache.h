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

#ifndef THREADSAFENETWORKDISKCACHE_H
#define THREADSAFENETWORKDISKCACHE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractNetworkCache>
#include <QMutex>
#include <QUrl>
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

 public Q_SLOTS:
  void clear() override;

 private:
  static QMutex sMutex;
  static int sInstances;
  static QNetworkDiskCache *sCache;
};

#endif  // THREADSAFENETWORKDISKCACHE_H
