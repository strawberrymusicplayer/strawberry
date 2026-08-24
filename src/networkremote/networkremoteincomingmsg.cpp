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

using namespace nwr_types;

NetworkRemoteIncomingMsg::NetworkRemoteIncomingMsg(QObject *parent)
    : QObject(parent),
      socket_(nullptr) {}

NetworkRemoteIncomingMsg::~NetworkRemoteIncomingMsg() = default;

void NetworkRemoteIncomingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
  QObject::connect(socket_, &QIODevice::readyRead, this, &NetworkRemoteIncomingMsg::ReadyRead);
}

void NetworkRemoteIncomingMsg::SetMsgType() {
  msg_type_ = msg_.type();
}

MsgType NetworkRemoteIncomingMsg::GetMsgType() {
  return msg_type_;
}

nwr::RequestPlaylistSongs NetworkRemoteIncomingMsg::GetRequestPlaylistSongs() {
  return msg_.requestPlaylistSongs();
}

nwr::RequestPlaySong NetworkRemoteIncomingMsg::GetRequestPlaySong() {
  return msg_.requestPlaySong();
}

nwr::RequestAddSongToPlaylist NetworkRemoteIncomingMsg::GetRequestAddSongToPlaylist() {
  return msg_.requestAddSongToPlaylist();
}

nwr::RequestRemoveSongFromPlaylist NetworkRemoteIncomingMsg::GetRequestRemoveSongFromPlaylist() {
  return msg_.requestRemoveSongFromPlaylist();
}

void NetworkRemoteIncomingMsg::ReadyRead() {
  framer_.Feed(socket_->readAll());
  QByteArray payload;

  while (true) {
    const NetworkRemoteMessageFramer::Status status = framer_.NextFrame(payload);

    if (status == NetworkRemoteMessageFramer::Status::NeedMoreData) {
      break;
    }
    if (status == NetworkRemoteMessageFramer::Status::OversizedFrame) {
      qLog(Warning) << "Message length exceeds limit; dropping connection";
      framer_.Clear();
      socket_->disconnectFromHost();
      return;
    }

    msg_ = nwr::Message();
    QProtobufSerializer serializer;
    msg_.deserialize(&serializer, payload);
    if (serializer.lastError() == QAbstractProtobufSerializer::Error::None) {
      SetMsgType();
      Q_EMIT InMsgParsed();
    }
    else {
      qLog(Warning) << "Failed to deserialize message: ("<< qToUnderlying(serializer.lastError()) << ") " << serializer.lastErrorString();
    }
  }
}

quint32 NetworkRemoteIncomingMsg::GetMsgVersion() {
  return msg_.version();
}

QString NetworkRemoteIncomingMsg::GetClientName() {
  return msg_.requestConnect().clientName();
}

