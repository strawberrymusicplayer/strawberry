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
#include <QNetworkInterface>
#include "networkremote/networkremote.h"
#include "core/application.h"
#include "core/logging.h"

NetworkRemote::NetworkRemote(const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      enabled_(false),
      remote_port_(8888),
      server_(nullptr),
      settings_(new NetworkRemoteSettings()) {
  setObjectName("NetworkRemote");
}

NetworkRemote::~NetworkRemote() {
  StopTcpServer();
  delete settings_;
}

void NetworkRemote::Init() {
  qLog(Debug) << "NetworkRemote Init() ";
  LoadSettings();
  if (enabled_) {
    StartTcpServer();
  }
  else {
    StopTcpServer();
  }
}

void NetworkRemote::Update() {
  LoadSettings();
  if (enabled_) {
    StopTcpServer();
    StartTcpServer();
  }
  else {
    StopTcpServer();
  }
  qLog(Debug) << "NetworkRemote Updated ==== ";
}

void NetworkRemote::LoadSettings() {
  settings_->Load();
  enabled_ = settings_->UseRemote();
  remote_port_ = settings_->GetPort();
}

QNetworkAddressEntry NetworkRemote::DetectLocalAddressEntry() {
  for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
    if (!(interface.flags() & QNetworkInterface::IsUp) || interface.flags() & QNetworkInterface::IsLoopBack) {
      continue;
    }
    for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
      if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
        return entry;
      }
    }
  }
  return QNetworkAddressEntry();
}

QHostAddress NetworkRemote::DetectLocalIpAddress() {
    return DetectLocalAddressEntry().ip();
}

void NetworkRemote::StartTcpServer() {
  if (server_) {
    server_->StopServer();
    delete server_;
    server_ = nullptr;
  }
  const QNetworkAddressEntry entry = DetectLocalAddressEntry();
  ipAddr_ = entry.ip();
  server_ = new NetworkRemoteTcpServer(player_, this);
  server_->StartServer(ipAddr_, remote_port_, entry);
  qLog(Debug) << "TcpServer started";
}

void NetworkRemote::StopTcpServer() {
  if (server_) {
    server_->StopServer();
  }
}
