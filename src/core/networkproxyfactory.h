/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef NETWORKPROXYFACTORY_H
#define NETWORKPROXYFACTORY_H

#include "config.h"

#include <QtGlobal>
#include <QMutex>
#include <QList>
#include <QString>
#include <QUrl>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>

class NetworkProxyFactory : public QNetworkProxyFactory {
 public:
  // These values are persisted
  enum class Mode {
    System = 0,
    Direct = 1,
    Manual = 2
  };

  static NetworkProxyFactory *Instance();
  static const char *kSettingsGroup;

  // These methods are thread-safe
  void ReloadSettings();
  QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override;

 private:
  explicit NetworkProxyFactory();

  static NetworkProxyFactory *sInstance;

  QMutex mutex_;

  Mode mode_;
  QNetworkProxy::ProxyType type_;
  QString hostname_;
  quint64 port_;
  bool use_authentication_;
  QString username_;
  QString password_;

#ifdef Q_OS_LINUX
  QUrl env_url_;
#endif
};

#endif  // NETWORKPROXYFACTORY_H
