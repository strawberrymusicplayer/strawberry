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
      socket_(nullptr){}

NetworkRemoteIncomingMsg::~NetworkRemoteIncomingMsg() = default;

void NetworkRemoteIncomingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &NetworkRemoteIncomingMsg::ReadyRead);
}

void NetworkRemoteIncomingMsg::SetMsgType() {
  msg_type_ = msg_.type();
}

nw::remote::MsgTypeGadget::MsgType NetworkRemoteIncomingMsg::GetMsgType() {
  return msg_type_;
}

void NetworkRemoteIncomingMsg::ReadyRead() {
  qLog(Debug) << "Ready To Read";
  msg_stream_.append(socket_->readAll());
  constexpr quint32 kMaxMsgLen = 1024 * 1024; // 1 MiB

  while (true) {
    if (msg_stream_.size() < 4) {
      break;
    }

    QDataStream len_stream(msg_stream_.left(4));
    len_stream.setByteOrder(QDataStream::BigEndian);
    quint32 msg_len = 0;
    len_stream >> msg_len;

    if (msg_len > kMaxMsgLen) {
      qLog(Warning) << "Message length" << msg_len << "exceeds limit; dropping connection";
      msg_stream_.clear();
      socket_->disconnectFromHost();
      break;
    }

    if (static_cast<quint64>(msg_stream_.size()) < 4ULL + msg_len) {
      // Payload hasn't fully arrived yet.
      break;
    }

    const QByteArray complete_msg = msg_stream_.mid(4, msg_len);
    msg_stream_.remove(0, 4 + msg_len);
    msg_ = nw::remote::Message();
    QProtobufSerializer serializer;
    msg_.deserialize(&serializer, complete_msg);
    if (serializer.lastError() == QAbstractProtobufSerializer::Error::None) {
      SetMsgType();
      Q_EMIT InMsgParsed();
    }
    else {
      qLog(Warning) << "Failed to deserialize message: ("
            << qToUnderlying(serializer.lastError()) << ") " << serializer.lastErrorString();
    }
  }
}