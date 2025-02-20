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

#ifndef NETWORKREMOTEINCOMINGMSG_H
#define NETWORKREMOTEINCOMINGMSG_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include "networkremote/RemoteMessages.pb.h"

class QTcpSocket;

class NetworkRemoteIncomingMsg : public QObject{
  Q_OBJECT
 public:
  explicit NetworkRemoteIncomingMsg(QObject *parent = nullptr);
  ~NetworkRemoteIncomingMsg(); 
  void Init(QTcpSocket* socket);
  void SetMsgType();
  qint32 GetMsgType();

 private Q_SLOTS:
  void ReadyRead();

 Q_SIGNALS:
  void InMsgParsed();

 private:
  nw::remote::Message *msg_;
  QTcpSocket *socket_;
  long bytes_in_;
  QByteArray msg_stream_;
  std::string msg_string_;
  qint32 msg_type_;
};

#endif
