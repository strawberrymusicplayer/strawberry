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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QMutex>

#include "threadsafenetworkdiskcachedevice.h"

ThreadSafeNetworkDiskCacheDevice::ThreadSafeNetworkDiskCacheDevice(QIODevice *cache_device, QMutex *mutex, QObject *parent)
    : QIODevice(parent),
      cache_device_(cache_device),
      mutex_(mutex) {

  open(QIODevice::WriteOnly | QIODevice::Unbuffered);

}

qint64 ThreadSafeNetworkDiskCacheDevice::readData(char *data, const qint64 maxlen) {

  Q_UNUSED(data)
  Q_UNUSED(maxlen)

  return -1;

}

qint64 ThreadSafeNetworkDiskCacheDevice::writeData(const char *data, const qint64 len) {

  QMutexLocker l(mutex_);

  if (cache_device_) {
    return cache_device_->write(data, len);
  }

  // The saving was cancelled.
  return len;

}
