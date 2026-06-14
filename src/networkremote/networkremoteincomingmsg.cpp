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
#include <QProtobufSerializer>

#include "networkremoteincomingmsg.h"
#include "core/logging.h"

NetworkRemoteIncomingMsg::NetworkRemoteIncomingMsg(QObject *parent)
    : QObject(parent),
      socket_(nullptr),
      bytes_in_(0) {}

NetworkRemoteIncomingMsg::~NetworkRemoteIncomingMsg() = default;

void NetworkRemoteIncomingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &NetworkRemoteIncomingMsg::ReadyRead);
}

void NetworkRemoteIncomingMsg::SetMsgType() {
  QProtobufSerializer serializer;
  msg_.deserialize(&serializer, msg_stream_.constData());
  if (serializer.lastError() == QAbstractProtobufSerializer::Error::None) {
    msg_type_ = msg_.type();
    Q_EMIT InMsgParsed();
  } else {
    qLog(Debug) << "Failed to deserialize message: ("
      << qToUnderlying(serializer.lastError()) << ") "<< serializer.lastErrorString();
    }
}

nw::remote::MsgTypeGadget::MsgType NetworkRemoteIncomingMsg::GetMsgType() {
    return msg_type_;
}

void NetworkRemoteIncomingMsg::ReadyRead() {
    qLog(Debug) << "Ready To Read";
    msg_stream_ = socket_->readAll();
    if (!msg_stream_.isEmpty()) {
        SetMsgType();
    }
}
