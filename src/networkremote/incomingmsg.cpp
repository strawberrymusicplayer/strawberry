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

#include <QTcpSocket>
#include "incomingmsg.h"
#include "networkremote/RemoteMessages.pb.h"
#include "core/logging.h"

NetworkRemoteIncomingMsg::NetworkRemoteIncomingMsg(QObject *parent)
    : QObject(parent),
      msg_(new nw::remote::Message),
      socket_(nullptr),
      bytes_in_(0),
      msg_type_(0) {}

NetworkRemoteIncomingMsg::~NetworkRemoteIncomingMsg() {
  delete msg_;
}

void NetworkRemoteIncomingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &NetworkRemoteIncomingMsg::ReadyRead);
}

void NetworkRemoteIncomingMsg::SetMsgType() {
  msg_string_ = msg_stream_.toStdString();
  msg_->ParseFromString(msg_string_);
  Q_EMIT InMsgParsed();
}

qint32 NetworkRemoteIncomingMsg::GetMsgType() {
  return msg_->type();
}

void NetworkRemoteIncomingMsg::ReadyRead() {
  qLog(Debug) << "Ready To Read";
  msg_stream_ = socket_->readAll();
  if (msg_stream_.length() > 0) SetMsgType();
}
