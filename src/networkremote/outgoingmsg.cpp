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
#include "outgoingmsg.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"

NetworkRemoteOutgoingMsg::NetworkRemoteOutgoingMsg(const SharedPtr<Player>& player, QObject *parent)
    : QObject(parent),  
    msg_(new nw::remote::Message),
    response_song_(new nw::remote::ResponseSongMetadata),
    player_(player) {}

void NetworkRemoteOutgoingMsg::Init(QTcpSocket *socket) {
  socket_ = socket;
}

void NetworkRemoteOutgoingMsg::SendCurrentTrackInfo() {
  msg_->Clear();
  song_ = new nw::remote::SongMetadata;
  response_song_->Clear();
  current_item_ = player_->GetCurrentItem();

  if (current_item_ != nullptr) {
    song_->mutable_title()->assign(current_item_->EffectiveMetadata().PrettyTitle().toStdString());
    song_->mutable_album()->assign(current_item_->EffectiveMetadata().album().toStdString());
    song_->mutable_artist()->assign(current_item_->EffectiveMetadata().artist().toStdString());
    song_->mutable_albumartist()->assign(current_item_->EffectiveMetadata().albumartist().toStdString());
    song_->set_track(current_item_->EffectiveMetadata().track());
    song_->mutable_stryear()->assign(current_item_->EffectiveMetadata().PrettyYear().toStdString());
    song_->mutable_genre()->assign(current_item_->EffectiveMetadata().genre().toStdString());
    song_->set_playcount(current_item_->EffectiveMetadata().playcount());
    song_->mutable_songlength()->assign(current_item_->EffectiveMetadata().PrettyLength().toStdString());
    msg_->set_type(nw::remote::MSG_TYPE_REPLY_SONG_INFO);
    msg_->mutable_response_song_metadata()->set_player_state(nw::remote::PLAYER_STATUS_PLAYING);
    msg_->mutable_response_song_metadata()->set_allocated_song_metadata(song_);
  }
  else {
    qInfo("I cannnot figure out how to get the song data if the song isn't playing");
    /* NOTE:  TODO
     *
     * */
    msg_->set_type(nw::remote::MSG_TYPE_UNSPECIFIED);
    msg_->mutable_response_song_metadata()->set_player_state(nw::remote::PLAYER_STATUS_UNSPECIFIED);
  }
  SendMsg();
}

void NetworkRemoteOutgoingMsg::SendMsg() {
  std::string  msgOut;
  msg_->SerializeToString(&msgOut);
  bytes_out_ = msg_->ByteSizeLong();
  if (socket_->isWritable()) {
    socket_->write(QByteArray::fromStdString(msgOut));
    qLog(Debug) << socket_->bytesToWrite() << " bytes written to socket " << socket_->socketDescriptor();
    msg_->Clear();
  }
}
