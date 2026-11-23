/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef THREADSAFENETWORKDISKCACHEDEVICE_H
#define THREADSAFENETWORKDISKCACHEDEVICE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QPointer>

class QMutex;

// The device a reply saves a URL with, which writes to the device of QNetworkDiskCache.
// QNetworkDiskCache deletes its device when the saving is cancelled with remove(), which the replies also call when they don't save the URL, for example when a POST invalidates the URL, or when a reply fails.
// So the reply saving the URL could still be writing to it, this device is kept until the reply is done, and only stops writing to the cache.
// It's not closed, the reply would start saving again with the rest of the data then.
class ThreadSafeNetworkDiskCacheDevice : public QIODevice {
  Q_OBJECT

 public:
  explicit ThreadSafeNetworkDiskCacheDevice(QIODevice *cache_device, QMutex *mutex, QObject *parent = nullptr);

  // Called with the mutex locked, QNetworkDiskCache deletes the device in another thread.
  QIODevice *cache_device() const { return cache_device_; }

  bool isSequential() const override { return true; }

 protected:
  qint64 readData(char *data, const qint64 maxlen) override;
  qint64 writeData(const char *data, const qint64 len) override;

 private:
  QPointer<QIODevice> cache_device_;
  QMutex *mutex_;
};

#endif  // THREADSAFENETWORKDISKCACHEDEVICE_H
