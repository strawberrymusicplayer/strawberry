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
#include "networkremotesettings.h"
#include "core/logging.h"


const char *NetworkRemoteSettings::kSettingsGroup = "NetworkRemote";

NetworkRemoteSettings::NetworkRemoteSettings()
    : enabled_(false),
      remote_port_(8888) {}

NetworkRemoteSettings::~NetworkRemoteSettings() {}

void NetworkRemoteSettings::Load() {  
  settings_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  if (!settings_.contains("useRemote")) {
    qLog(Debug) << "First time run the Network Remote";
    settings_.setValue("useRemote", false);
    settings_.setValue("remotePort", 8888);
  }
  else {
    enabled_ = settings_.value("useRemote").toBool();
    remote_port_ = settings_.value("remotePort").toInt();
  }
  settings_.endGroup();
  qLog(Debug) << "QSettings Loaded ++++++++++++++++";
}

void NetworkRemoteSettings::Save() {
  settings_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  settings_.setValue("useRemote", enabled_);
  settings_.setValue("remotePort", remote_port_);
  settings_.endGroup();
  settings_.sync();
  qLog(Debug) << "Saving QSettings ++++++++++++++++";
}

bool NetworkRemoteSettings::UseRemote() const {
  return enabled_;
}

int NetworkRemoteSettings::GetPort() const {
  return remote_port_;
}

void NetworkRemoteSettings::SetUseRemote(bool useRemote) {
  enabled_ = useRemote;
}


void NetworkRemoteSettings::SetPort(int port) {
  remote_port_ = port;
}
