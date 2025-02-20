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

#include "client.h"

NetworkRemoteClient::NetworkRemoteClient(const SharedPtr<Player>& player, QObject *parent)
    : QObject(parent),
      player_(player),
      incoming_msg_(new NetworkRemoteIncomingMsg(this)),
      outgoing_msg_(new NetworkRemoteOutgoingMsg(player, this)) {}

NetworkRemoteClient::~NetworkRemoteClient(){}

void NetworkRemoteClient::Init(QTcpSocket *socket){
  socket_ = socket;
  QObject::connect(incoming_msg_,&NetworkRemoteIncomingMsg::InMsgParsed,this, &NetworkRemoteClient::ProcessIncoming);
  incoming_msg_->Init(socket_);
  outgoing_msg_->Init(socket_);
}

QTcpSocket* NetworkRemoteClient::GetSocket() {
  return socket_;
}

void NetworkRemoteClient::ProcessIncoming() {
  switch (incoming_msg_->GetMsgType()) {
    case nw::remote::MSG_TYPE_REQUEST_SONG_INFO:
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PLAY:
      player_->Play();
      // In case the player was paused when the client started send the song info again
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_NEXT:
      player_->Next();
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PREVIOUS:
      player_->Previous();
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MSG_TYPE_REQUEST_PAUSE:
      player_->Pause();
      break;
    case nw::remote::MSG_TYPE_REQUEST_STOP:
      break;
    case nw::remote::MSG_TYPE_REQUEST_FINISH:
      Q_EMIT ClientIsLeaving();
      break;
    case nw::remote::MSG_TYPE_DISCONNECT:
      break;
    default:
        qInfo("Unknown mwessage type");
      break;
  }
}
