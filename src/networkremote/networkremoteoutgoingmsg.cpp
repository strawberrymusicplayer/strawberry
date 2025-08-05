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
#include "networkremoteoutgoingmsg.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"

NetworkRemoteOutgoingMsg::NetworkRemoteOutgoingMsg(const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      bytes_out_(0),
      msg_type_(0),
      playlist_(nullptr),
      socket_(nullptr) {}

void NetworkRemoteOutgoingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
}

void NetworkRemoteOutgoingMsg::SendCurrentTrackInfo() {
  msg_ =  nw::remote::Message();
  response_song_ = nw::remote::ResponseSongMetadata();
  current_item_ = player_->GetCurrentItem();

  if (current_item_ != nullptr) {
    song_ = nw::remote::SongMetadata();
    song_.setTitle(current_item_->EffectiveMetadata().PrettyTitle());
    song_.setAlbum(current_item_->EffectiveMetadata().album());
    song_.setArtist(current_item_->EffectiveMetadata().artist());
    song_.setAlbumartist(current_item_->EffectiveMetadata().albumartist());
    song_.setTrack(current_item_->EffectiveMetadata().track());
    song_.setStryear(current_item_->EffectiveMetadata().PrettyYear());
    song_.setGenre(current_item_->EffectiveMetadata().genre());
    song_.setPlaycount(current_item_->EffectiveMetadata().playcount());
    song_.setSonglength(current_item_->EffectiveMetadata().PrettyLength());

    response_song_.setPlayerState(nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_PLAYING);
    response_song_.setSongMetadata(song_);

    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REPLY_SONG_INFO);
    msg_.setResponseSongMetadata(response_song_);
  }
  else {
    response_song_.setPlayerState(nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_UNSPECIFIED);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_REPLY_SONG_INFO);
    msg_.setResponseSongMetadata(response_song_);
  }
  SendMsg();
}

void NetworkRemoteOutgoingMsg::SendMsg() {
  QProtobufSerializer serializer;
  QByteArray data = serializer.serialize(&msg_);
  if (serializer.lastError() != QAbstractProtobufSerializer::Error::None) {
    qLog(Debug) << "Failed to serialize message:" << serializer.lastErrorString();
    return;
  }
  bytes_out_ = data.size();
  if (socket_ && socket_->isWritable()) {
    socket_->write(data);
    qLog(Debug) << bytes_out_ << "bytes written to socket" << socket_->socketDescriptor();
  } else {
    qWarning() << "Socket is not writable.";
  }
}
