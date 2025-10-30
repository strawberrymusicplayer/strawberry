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

#ifndef NETWORKREMOTETCPSERVER_H
#define NETWORKREMOTETCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "networkremote/networkremoteclientmanager.h"

class NetworkRemoteTcpServer : public QObject{
  Q_OBJECT
 public:
  explicit NetworkRemoteTcpServer(const SharedPtr<Player> player, QObject *parent = nullptr);
  bool ServerUp();

 public Q_SLOTS:
  void NewTcpConnection();
  void StartServer(QHostAddress ipAddr, int port);
  void StopServer();

 private:
  const SharedPtr<Player> player_;
  QTcpServer *server_;
  QTcpSocket *socket_;
  NetworkRemoteClientManager *client_mgr_;
};

#endif
