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

#include <QNetworkProxy>
#include "tcpserver.h"
#include "core/logging.h"
#include "networkremote/clientmanager.h"

NetworkRemoteTcpServer::NetworkRemoteTcpServer(const SharedPtr<Player>& player, QObject *parent)
    : QObject(parent),
      player_(player),
      server_(new QTcpServer(this)),
      client_mgr_(new NetworkRemoteClientManager(player_,this)) {
  connect(server_,&QTcpServer::newConnection, this, &NetworkRemoteTcpServer::NewTcpConnection);
}

void NetworkRemoteTcpServer::StartServer(QHostAddress ipAddr, int port) {
  server_->setProxy(QNetworkProxy::NoProxy);
  if (server_->listen(ipAddr, port)) {
    qLog(Debug) << "TCP Server Started on --- " << ipAddr.toString() << " and port -- " << port;
  }
}

void NetworkRemoteTcpServer::NewTcpConnection() {
  socket_ = server_->nextPendingConnection();
  client_mgr_->AddClient(socket_);
  qLog(Debug) << "New Socket -------------------";
}

void NetworkRemoteTcpServer::StopServer() {
  if(server_ && server_->isListening()){
    server_->close();
    qLog(Debug) << "TCP Server Stopped ----------------------";
  }
}

bool NetworkRemoteTcpServer::ServerUp() {
  return server_->isListening();
}
