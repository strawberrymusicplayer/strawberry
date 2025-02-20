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

#ifndef NETWORKREMOTECLIENT_H
#define NETWORKREMOTECLIENT_H

#include <QObject>
#include <QTcpSocket>
#include "incomingmsg.h"
#include "outgoingmsg.h"
#include "core/player.h"

class NetworkRemoteClient : public QObject{
  Q_OBJECT
 public:
  explicit NetworkRemoteClient(const SharedPtr<Player>&  player, QObject *parent = nullptr);
  ~NetworkRemoteClient();
  void Init(QTcpSocket*);
  QTcpSocket *GetSocket();
  void ProcessIncoming();

 Q_SIGNALS:
  void ReceiveMsg();
  void PrepareResponse();
  void ClientIsLeaving();

 private:
  const SharedPtr<Player> player_;
  QTcpSocket *socket_;
  NetworkRemoteIncomingMsg *incoming_msg_;
  NetworkRemoteOutgoingMsg *outgoing_msg_;
};

#endif
