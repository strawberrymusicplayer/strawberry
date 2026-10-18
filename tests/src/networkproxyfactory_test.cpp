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

#include "gtest_include.h"

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkProxy>

#include "core/networkproxyfactory.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

TEST(NetworkProxyFactoryTest, ProxyFromUrlHttp) {

  const QNetworkProxy proxy = NetworkProxyFactory::ProxyFromUrl(u"http://user:p%40ss@proxy.example.com:3128/"_s);
  EXPECT_EQ(QNetworkProxy::HttpProxy, proxy.type());
  EXPECT_EQ(u"proxy.example.com"_s, proxy.hostName());
  EXPECT_EQ(3128, proxy.port());
  EXPECT_EQ(u"user"_s, proxy.user());
  EXPECT_EQ(u"p@ss"_s, proxy.password());

}

TEST(NetworkProxyFactoryTest, ProxyFromUrlWithoutScheme) {

  const QNetworkProxy proxy = NetworkProxyFactory::ProxyFromUrl(u"proxy.example.com:3128"_s);
  EXPECT_EQ(QNetworkProxy::HttpProxy, proxy.type());
  EXPECT_EQ(u"proxy.example.com"_s, proxy.hostName());
  EXPECT_EQ(3128, proxy.port());

}

TEST(NetworkProxyFactoryTest, ProxyFromUrlWithoutPort) {

  EXPECT_EQ(8080, NetworkProxyFactory::ProxyFromUrl(u"http://proxy.example.com"_s).port());
  EXPECT_EQ(8080, NetworkProxyFactory::ProxyFromUrl(u"proxy.example.com"_s).port());

  const QNetworkProxy socks_proxy = NetworkProxyFactory::ProxyFromUrl(u"socks5h://proxy.example.com"_s);
  EXPECT_EQ(QNetworkProxy::Socks5Proxy, socks_proxy.type());
  EXPECT_EQ(1080, socks_proxy.port());

}

TEST(NetworkProxyFactoryTest, ProxyFromUrlUnsupported) {

  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrl(QString()).type());
  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrl(u"socks4://proxy.example.com:1080"_s).type());
  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrl(u"ftp://proxy.example.com:21"_s).type());

}

TEST(NetworkProxyFactoryTest, NoProxyDomains) {

  const QString no_proxy = u"localhost, .example.com,*.example.org,example.net:8080"_s;
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"localhost"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"example.com"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"www.Example.com"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"www.example.org"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"example.net"_s, 8080, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"notexample.com"_s, -1, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"example.de"_s, -1, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(QString(), -1, no_proxy));

}

TEST(NetworkProxyFactoryTest, NoProxyAddresses) {

  const QString no_proxy = u"127.0.0.1,10.0.0.0/8,::1,[fe80::1]:80,192.168.1.1:8080"_s;
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"127.0.0.1"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"10.1.2.3"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"::1"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"[::1]"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"fe80::1"_s, 80, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"192.168.1.1"_s, 8080, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"11.1.2.3"_s, -1, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"127.0.0.2"_s, -1, no_proxy));

}

TEST(NetworkProxyFactoryTest, NoProxyWildcard) {

  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"www.example.com"_s, -1, u"*"_s));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"www.example.com"_s, -1, QString()));

}

TEST(NetworkProxyFactoryTest, ProxyFromUrlsSkipsUnsupported) {

  // An unsupported proxy URL in the first variable doesn't hide a supported one in a later variable.
  const QNetworkProxy proxy = NetworkProxyFactory::ProxyFromUrls(QStringList() << QString() << u"socks4://socks.example.com"_s << u"http://proxy.example.com:3128"_s);
  EXPECT_EQ(QNetworkProxy::HttpProxy, proxy.type());
  EXPECT_EQ(u"proxy.example.com"_s, proxy.hostName());
  EXPECT_EQ(3128, proxy.port());

}

TEST(NetworkProxyFactoryTest, ProxyFromUrlsWithoutProxy) {

  // NoProxy, not DefaultProxy, which is the type of a default constructed QNetworkProxy.
  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrls(QStringList()).type());
  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrls(QStringList() << QString() << QString()).type());
  EXPECT_EQ(QNetworkProxy::NoProxy, NetworkProxyFactory::ProxyFromUrls(QStringList() << u"socks4://socks.example.com"_s).type());

}

TEST(NetworkProxyFactoryTest, NoProxyPorts) {

  // Entries with a port only match that port, entries without a port match all ports.
  const QString no_proxy = u"example.net:8080,192.168.1.1:8080,[fe80::1]:80,example.org"_s;
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"example.net"_s, 8080, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"example.net"_s, 443, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"example.net"_s, -1, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"192.168.1.1"_s, 8080, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"192.168.1.1"_s, 80, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"fe80::1"_s, 80, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"[fe80::1]"_s, 80, no_proxy));
  EXPECT_FALSE(NetworkProxyFactory::HostMatchesNoProxy(u"fe80::1"_s, 443, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"example.org"_s, 443, no_proxy));
  EXPECT_TRUE(NetworkProxyFactory::HostMatchesNoProxy(u"example.org"_s, -1, no_proxy));

}

TEST(NetworkProxyFactoryTest, QueryPort) {

  EXPECT_EQ(443, NetworkProxyFactory::QueryPort(QNetworkProxyQuery(QUrl(u"https://example.net/"_s))));
  EXPECT_EQ(80, NetworkProxyFactory::QueryPort(QNetworkProxyQuery(QUrl(u"http://example.net/"_s))));
  EXPECT_EQ(8080, NetworkProxyFactory::QueryPort(QNetworkProxyQuery(QUrl(u"https://example.net:8080/"_s))));
  EXPECT_EQ(-1, NetworkProxyFactory::QueryPort(QNetworkProxyQuery(QUrl(u"ftp://example.net/"_s))));

}

#ifdef Q_OS_LINUX
// The factory reads the environment variables when it's created, this is the only test using it.
TEST(NetworkProxyFactoryTest, SystemModeWithoutProxyEnvironmentVariables) {

  qunsetenv("HTTP_PROXY");
  qunsetenv("http_proxy");
  qunsetenv("ALL_PROXY");
  qunsetenv("all_proxy");

  // The settings are empty in the tests, so the default system mode is used.
  const QList<QNetworkProxy> proxies = NetworkProxyFactory::Instance()->queryProxy(QNetworkProxyQuery(QUrl(u"https://www.example.com/"_s)));
  ASSERT_EQ(1, proxies.count());
  EXPECT_EQ(QNetworkProxy::NoProxy, proxies.first().type());

}
#endif

}  // namespace
