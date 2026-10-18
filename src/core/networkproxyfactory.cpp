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
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkProxy>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSettings>

#include "constants/networkproxysettings.h"
#include "core/logging.h"
#include "core/settings.h"
#include "networkproxyfactory.h"

using namespace Qt::Literals::StringLiterals;

NetworkProxyFactory *NetworkProxyFactory::sInstance = nullptr;

NetworkProxyFactory::NetworkProxyFactory()
    : mode_(NetworkProxySettings::Mode::System),
      type_(QNetworkProxy::HttpProxy),
      port_(8080),
      use_authentication_(false) {

#ifdef Q_OS_LINUX
  // Linux uses environment variables to pass proxy configuration information, which systemProxyForQuery doesn't support for some reason.

  const QStringList urls = QStringList() << QString::fromLocal8Bit(qgetenv("HTTP_PROXY"))
                                         << QString::fromLocal8Bit(qgetenv("http_proxy"))
                                         << QString::fromLocal8Bit(qgetenv("ALL_PROXY"))
                                         << QString::fromLocal8Bit(qgetenv("all_proxy"));

  // A default constructed QNetworkProxy is DefaultProxy, ProxyFromUrls() returns NoProxy when no proxy is set.
  env_proxy_ = ProxyFromUrls(urls);
  if (env_proxy_.type() != QNetworkProxy::NoProxy) {
    // Don't log the password.
    qLog(Debug) << "Detected system proxy:" << env_proxy_.type() << env_proxy_.hostName() << env_proxy_.port();
  }

  env_no_proxy_ = QString::fromLocal8Bit(qgetenv("no_proxy"));
  if (env_no_proxy_.isEmpty()) {
    env_no_proxy_ = QString::fromLocal8Bit(qgetenv("NO_PROXY"));
  }
#endif

  ReloadSettings();

}

NetworkProxyFactory *NetworkProxyFactory::Instance() {

  if (!sInstance) {
    sInstance = new NetworkProxyFactory;
  }

  return sInstance;

}

void NetworkProxyFactory::ReloadSettings() {

  QMutexLocker l(&mutex_);

  Settings s;
  s.beginGroup(NetworkProxySettings::kSettingsGroup);

  mode_ = static_cast<NetworkProxySettings::Mode>(s.value(NetworkProxySettings::kMode, static_cast<int>(NetworkProxySettings::kDefaultMode)).toInt());
  type_ = QNetworkProxy::ProxyType(s.value(NetworkProxySettings::kType, NetworkProxySettings::kDefaultType).toInt());
  hostname_ = s.value(NetworkProxySettings::kHostname).toString();
  port_ = s.value(NetworkProxySettings::kPort, NetworkProxySettings::kDefaultPort).toULongLong();
  use_authentication_ = s.value(NetworkProxySettings::kUseAuthentication, NetworkProxySettings::kDefaultUseAuthentication).toBool();
  username_ = s.value(NetworkProxySettings::kUsername).toString();
  password_ = s.value(NetworkProxySettings::kPassword).toString();

  s.endGroup();

}

QList<QNetworkProxy> NetworkProxyFactory::queryProxy(const QNetworkProxyQuery &query) {

  QMutexLocker l(&mutex_);

  QNetworkProxy ret;

  switch (mode_) {
    case NetworkProxySettings::Mode::System:
#ifdef Q_OS_LINUX
      if (env_proxy_.type() == QNetworkProxy::NoProxy || HostMatchesNoProxy(query.peerHostName(), QueryPort(query), env_no_proxy_)) {
        ret.setType(QNetworkProxy::NoProxy);
      }
      else {
        ret = env_proxy_;
      }
      break;
#else
      return systemProxyForQuery(query);
#endif

    case NetworkProxySettings::Mode::Direct:
      ret.setType(QNetworkProxy::NoProxy);
      break;

    case NetworkProxySettings::Mode::Manual:
      ret.setType(type_);
      ret.setHostName(hostname_);
      ret.setPort(port_);
      if (use_authentication_) {
        ret.setUser(username_);
        ret.setPassword(password_);
      }
      break;
  }

  return QList<QNetworkProxy>() << ret;

}

QNetworkProxy NetworkProxyFactory::ProxyFromUrl(const QString &url_str) {

  const QString url_trimmed = url_str.trimmed();
  if (url_trimmed.isEmpty()) return QNetworkProxy(QNetworkProxy::NoProxy);

  // Without a scheme, "proxy:3128" would be parsed as the scheme "proxy", so add http:// like curl does.
  const QUrl url(url_trimmed.contains("://"_L1) ? url_trimmed : "http://"_L1 + url_trimmed);
  if (!url.isValid() || url.host().isEmpty()) {
    qLog(Error) << "Invalid proxy URL" << url.toString(QUrl::RemoveUserInfo);
    return QNetworkProxy(QNetworkProxy::NoProxy);
  }

  const QString scheme = url.scheme().toLower();
  QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;
  int default_port = 0;
  if (scheme == "http"_L1 || scheme == "https"_L1) {
    type = QNetworkProxy::HttpProxy;
    default_port = 8080;
  }
  else if (scheme == "socks"_L1 || scheme == "socks5"_L1 || scheme == "socks5h"_L1) {
    type = QNetworkProxy::Socks5Proxy;
    default_port = 1080;
  }
  else {
    qLog(Error) << "Unsupported proxy URL scheme" << scheme;
    return QNetworkProxy(QNetworkProxy::NoProxy);
  }

  return QNetworkProxy(type, url.host(), static_cast<quint16>(url.port(default_port)), url.userName(), url.password());

}

QNetworkProxy NetworkProxyFactory::ProxyFromUrls(const QStringList &urls) {

  for (const QString &url_str : urls) {
    if (url_str.isEmpty()) continue;
    const QNetworkProxy proxy = ProxyFromUrl(url_str);
    if (proxy.type() != QNetworkProxy::NoProxy) return proxy;
  }

  return QNetworkProxy(QNetworkProxy::NoProxy);

}

int NetworkProxyFactory::QueryPort(const QNetworkProxyQuery &query) {

  if (query.peerPort() != -1) return query.peerPort();

  const QString scheme = query.protocolTag().isEmpty() ? query.url().scheme().toLower() : query.protocolTag().toLower();
  if (scheme == "http"_L1) return 80;
  if (scheme == "https"_L1) return 443;

  return -1;

}

bool NetworkProxyFactory::HostMatchesNoProxy(const QString &host, const int port, const QString &no_proxy) {

  if (host.isEmpty() || no_proxy.isEmpty()) return false;

  QString host_name = host.toLower();
  if (host_name.startsWith(u'[') && host_name.endsWith(u']')) host_name = host_name.mid(1, host_name.length() - 2);
  if (host_name.endsWith(u'.')) host_name.chop(1);
  const QHostAddress host_address(host_name);

  static const QRegularExpression regex_port(u":([0-9]+)$"_s);

  const QStringList entries = no_proxy.split(u',', Qt::SkipEmptyParts);
  for (const QString &entry_str : entries) {
    QString entry = entry_str.trimmed().toLower();
    if (entry.isEmpty()) continue;
    if (entry == "*"_L1) return true;

    // An entry with a port only matches that port, an entry without a port matches all ports.
    QString entry_port;
    if (entry.startsWith(u'[')) {
      const qsizetype end = entry.indexOf(u']');
      if (end == -1) continue;
      const QString rest = entry.mid(end + 1);
      if (!rest.isEmpty()) {
        const QRegularExpressionMatch match = regex_port.match(rest);
        if (!match.hasMatch() || match.capturedStart() != 0) continue;
        entry_port = match.captured(1);
      }
      entry = entry.mid(1, end - 1);
    }
    else if (entry.count(u':') <= 1) {
      // IPv6 addresses have more than one ':', and can't have a port without brackets.
      const QRegularExpressionMatch match = regex_port.match(entry);
      if (match.hasMatch()) {
        entry_port = match.captured(1);
        entry.truncate(match.capturedStart());
      }
    }
    if (!entry_port.isEmpty() && entry_port.toInt() != port) continue;

    if (!host_address.isNull()) {
      if (entry.contains(u'/')) {
        const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(entry);
        if (!subnet.first.isNull() && host_address.isInSubnet(subnet)) return true;
      }
      else if (QHostAddress(entry).isEqual(host_address, QHostAddress::TolerantConversion)) {
        return true;
      }
      continue;
    }

    // Domain names also match their subdomains, a leading "." or "*." is allowed.
    if (entry.startsWith("*."_L1)) entry.remove(0, 2);
    else if (entry.startsWith(u'.')) entry.remove(0, 1);
    if (entry.endsWith(u'.')) entry.chop(1);
    if (entry.isEmpty()) continue;
    if (host_name == entry || host_name.endsWith(u'.' + entry)) return true;
  }

  return false;

}
