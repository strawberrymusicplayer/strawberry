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

#ifndef NETWORK_H
#define NETWORK_H

#include "config.h"

#include <stdbool.h>

#include <QtGlobal>
#include <QObject>
#include <QNetworkAccessManager>
#include <QAbstractNetworkCache>
#include <QIODevice>
#include <QMutex>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>

class NetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT

 public:
  explicit NetworkAccessManager(QObject *parent = nullptr);

 protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData);
};

class ThreadSafeNetworkDiskCache : public QAbstractNetworkCache {
 public:
  explicit ThreadSafeNetworkDiskCache(QObject *parent);
  ~ThreadSafeNetworkDiskCache();

  qint64 cacheSize() const;
  QIODevice *data(const QUrl &url);
  void insert(QIODevice *device);
  QNetworkCacheMetaData metaData(const QUrl &url);
  QIODevice *prepare(const QNetworkCacheMetaData &metaData);
  bool remove(const QUrl &url);
  void updateMetaData(const QNetworkCacheMetaData &metaData);

  void clear();

 private:
  static QMutex sMutex;
  static ThreadSafeNetworkDiskCache *sInstance;
  static QNetworkDiskCache *sCache;
};

#endif  // NETWORK_H
