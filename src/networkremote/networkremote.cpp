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

#include <QThread>
#include "networkremote/networkremote.h"
#include "core/application.h"
#include "core/logging.h"

NetworkRemote::NetworkRemote(const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      enabled_(false),
      local_only_(false),
      remote_port_(5050),
      server_(nullptr),
      settings_(new NetworkRemoteSettings()) {
  setObjectName("NetworkRemote");
}

NetworkRemote::~NetworkRemote() {
  stopTcpServer();
}

void NetworkRemote::Init() {
  qLog(Debug) << "NetworkRemote Init() ";
  LoadSettings();
  if (enabled_) {
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
}

void NetworkRemote::Update() {
  LoadSettings();
  if (enabled_) {
    stopTcpServer();
    startTcpServer();
  }
  else {
    stopTcpServer();
  }
  qLog(Debug) << "NetworkRemote Updated ==== ";
}

void NetworkRemote::LoadSettings() {
  settings_->Load();
  enabled_ = settings_->UserRemote();
  local_only_ = settings_->LocalOnly();
  remote_port_ = settings_->GetPort();
  ipAddr_.setAddress(settings_->GetIpAddress());
}

void NetworkRemote::startTcpServer() {
  server_ = new NetworkRemoteTcpServer(player_, this);
  server_->StartServer(ipAddr_, remote_port_);
}

void NetworkRemote::stopTcpServer() {
  if (server_ && server_->ServerUp()) {
    qLog(Debug) << "TcpServer stopped ";
    server_->StopServer();
  }
}
