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
#include "constants/networkremoteconstants.h"

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
    const quint32 client_version = incoming_msg_->GetMsgVersion();
    if (client_version < NetworkRemoteConstants::kMinSupportedVersion) {
        qLog(Warning) << "Rejecting client with protocol version" << client_version
                      << "- minimum supported is" << NetworkRemoteConstants::kMinSupportedVersion;
        outgoing_msg_->SendDisconnect(nw::remote::ReasonDisconnectGadget::ReasonDisconnect::REASON_DISCONNECT_VERSION_MISMATCH);
        Q_EMIT ClientIsLeaving();
        return;   // don't process the message
    }

    const nw::remote::MsgTypeGadget::MsgType msg_type = incoming_msg_->GetMsgType();

    if (!handshake_complete_) {
        if (msg_type != nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_CONNECT) {
            qLog(Warning) << "Client sent message type" << static_cast<int>(msg_type) << "before handshake - disconnecting";
            outgoing_msg_->SendDisconnect(nw::remote::ReasonDisconnectGadget::ReasonDisconnect::REASON_DISCONNECT_NO_HANDSHAKE);
            Q_EMIT ClientIsLeaving();
            return;
        }
        handshake_complete_ = true;
        qLog(Debug) << "Handshake from client:" << incoming_msg_->GetClientName() << "protocol version" << client_version;
        outgoing_msg_->SendConnectResponse(true);
        return;
    }

    switch (msg_type) {
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
    case nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REQUEST_CONNECT:
        qLog(Warning) << "Duplicate handshake ignored";
        break;
    default:
        qLog(Debug) << "Unknown message type";
        outgoing_msg_->SendDisconnect(nw::remote::ReasonDisconnectGadget::ReasonDisconnect::REASON_DISCONNECT_UNKNOWN_MSGTYPE);
        Q_EMIT ClientIsLeaving();
        break;
    }
}

void NetworkRemoteClient::SendEngineState(EngineBase::State state) {
    outgoing_msg_->SendEngineState(state);
}

void NetworkRemoteClient::SendDisconnect(nw::remote::ReasonDisconnectGadget::ReasonDisconnect reason) {
    outgoing_msg_->SendDisconnect(reason);
}
