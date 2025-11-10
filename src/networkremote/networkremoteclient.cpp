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

#include "core/logging.h"
#include "networkremoteclient.h"
#include "core/player.h"

NetworkRemoteClient::NetworkRemoteClient(const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
    player_(player),
    incoming_msg_(new NetworkRemoteIncomingMsg(this)),
    outgoing_msg_(new NetworkRemoteOutgoingMsg(player, this)) {
    QObject::connect(this, &NetworkRemoteClient::RequestPlay, player_.get(), [this]() { player_->Play(); });
    QObject::connect(this, &NetworkRemoteClient::RequestPause, player_.get(), &Player::Pause);
    QObject::connect(this, &NetworkRemoteClient::RequestNext, player_.get(), &Player::Next);
    QObject::connect(this, &NetworkRemoteClient::RequestPrevious, player_.get(), &Player::Previous);
    QObject::connect(this, &NetworkRemoteClient::RequestStop, player_.get(), [this]() { player_->Stop(); });
}

NetworkRemoteClient::~NetworkRemoteClient(){}

void NetworkRemoteClient::Init(QTcpSocket *socket){
  socket_ = socket;
  QObject::connect(incoming_msg_, &NetworkRemoteIncomingMsg::InMsgParsed, this, &NetworkRemoteClient::ProcessIncoming);
  incoming_msg_->Init(socket_);
  outgoing_msg_->Init(socket_);
}

QTcpSocket *NetworkRemoteClient::GetSocket() {
  return socket_;
}

void NetworkRemoteClient::ProcessIncoming() {
  switch (incoming_msg_->GetMsgType()) {
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_SONG_INFO:
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_PLAY:
      Q_EMIT RequestPlay();
      outgoing_msg_->SendCurrentTrackInfo();
        break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_NEXT:
      Q_EMIT RequestNext();
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_PREVIOUS:
      Q_EMIT RequestPrevious();
      outgoing_msg_->SendCurrentTrackInfo();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_PAUSE:
      Q_EMIT RequestPause();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_STOP:
      Q_EMIT RequestStop();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_FINISH:
      Q_EMIT ClientIsLeaving();
      break;
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_DISCONNECT:
      Q_EMIT ClientIsLeaving();
      break;
    default:
      qLog(Debug) << "Unknown message type";
      break;
    }
}