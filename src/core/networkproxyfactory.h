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

#ifndef NETWORKPROXYFACTORY_H
#define NETWORKPROXYFACTORY_H

#include "config.h"

#include <QtGlobal>
#include <QMutex>
#include <QList>
#include <QString>
#include <QStringList>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>

#include "constants/networkproxysettings.h"

class NetworkProxyFactory : public QNetworkProxyFactory {
 public:
  static NetworkProxyFactory *Instance();

  // These methods are thread-safe
  void ReloadSettings();
  QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override;

  // Returns the proxy for a proxy URL from the environment, like http_proxy, or NoProxy if it's not a supported proxy URL.
  // URLs without a scheme are HTTP proxies, and URLs without a port use 8080 for HTTP proxies and 1080 for SOCKS proxies.
  static QNetworkProxy ProxyFromUrl(const QString &url_str);

  // Returns the proxy for the first supported proxy URL, skipping empty and unsupported URLs, or NoProxy if there is none.
  static QNetworkProxy ProxyFromUrls(const QStringList &urls);

  // Returns the port the query connects to, the default port for the scheme if the URL has no port, or -1 if it's not known.
  static int QueryPort(const QNetworkProxyQuery &query);

  // Returns true when the host should be connected to directly according to the no_proxy environment variable.
  // The entries are separated by commas, they can be * for all hosts, a domain name, which also matches subdomains, an IP address or a CIDR range, optionally with a port.
  // An entry with a port only matches that port, port is -1 when it's not known, then entries with a port don't match.
  static bool HostMatchesNoProxy(const QString &host, const int port, const QString &no_proxy);

 private:
  explicit NetworkProxyFactory();

  static NetworkProxyFactory *sInstance;

  QMutex mutex_;

  NetworkProxySettings::Mode mode_;
  QNetworkProxy::ProxyType type_;
  QString hostname_;
  quint64 port_;
  bool use_authentication_;
  QString username_;
  QString password_;

#ifdef Q_OS_LINUX
  QNetworkProxy env_proxy_;
  QString env_no_proxy_;
#endif
};

#endif  // NETWORKPROXYFACTORY_H
