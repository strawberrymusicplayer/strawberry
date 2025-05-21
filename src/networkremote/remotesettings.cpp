/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#include <QHostAddress>
#include <QNetworkInterface>
#include "remotesettings.h"
#include "core/logging.h"

const char *NetworkRemoteSettings::kSettingsGroup = "NetworkRemote";

NetworkRemoteSettings::NetworkRemoteSettings()
    : enabled_(false),
      local_only_(false),
      remote_port_(5050) {}

NetworkRemoteSettings::~NetworkRemoteSettings() {}

void NetworkRemoteSettings::Load() {
  SetIpAdress();
  s_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  if (!s_.contains("useRemote")) {
    qLog(Debug) << "First time run the Network Remote";
    s_.setValue("useRemote", false);
    s_.setValue("localOnly",false);
    s_.setValue("remotePort",5050);
    s_.setValue("ipAddress",ip_addr_);
  }
  else {
    enabled_ = s_.value("useRemote").toBool();
    local_only_ = s_.value("localOnly").toBool();
    remote_port_ = s_.value("remotePort").toInt();
    s_.setValue("ipAddress",ip_addr_);
  }
  s_.endGroup();
  qLog(Debug) << "QSettings Loaded ++++++++++++++++";
}

void NetworkRemoteSettings::Save() {
  s_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  s_.setValue("useRemote",enabled_);
  s_.setValue("localOnly",local_only_);
  s_.setValue("remotePort",remote_port_);
  s_.setValue("ipAddress",ip_addr_);
  s_.endGroup();
  s_.sync();
  qLog(Debug) << "Saving QSettings ++++++++++++++++";
}

bool NetworkRemoteSettings::UserRemote() {
  return enabled_;
}

bool NetworkRemoteSettings::LocalOnly() {
  return local_only_;
}

QString NetworkRemoteSettings::GetIpAddress() {
  return ip_addr_;
}

int NetworkRemoteSettings::GetPort() {
  return remote_port_;
}

void NetworkRemoteSettings::SetUseRemote(bool useRemote) {
  enabled_ = useRemote;
  Save();
}

void NetworkRemoteSettings::SetLocalOnly(bool localOnly) {
  local_only_ = localOnly;
  Save();
}

void NetworkRemoteSettings::SetIpAdress() {
  bool found = false;
  QList<QHostAddress> hostList = QNetworkInterface::allAddresses();
  for (const QHostAddress &address : std::as_const(hostList)) {
    if (address.protocol() == QAbstractSocket::IPv4Protocol && address.isLoopback() == false && !found) {
    qInfo( "Warning: The code only picks the first IPv4 address");
      found = true;
      ip_addr_ = address.toString();
    }
  }
}

void NetworkRemoteSettings::SetPort(int port) {
  remote_port_ = port;
  Save();
}
