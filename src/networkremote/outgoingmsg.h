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

#ifndef NETWORKREMOTEOUTGOINGMSG_H
#define NETWORKREMOTEOUTGOINGMSG_H

#include <QObject>
#include <QByteArray>
#include "playlist/playlistitem.h"
#include "includes/shared_ptr.h"
#include "networkremote/RemoteMessages.pb.h"

class Playlist;
class Player;
class QTcpSocket;

class NetworkRemoteOutgoingMsg : public QObject{
  Q_OBJECT
 public:
  explicit NetworkRemoteOutgoingMsg(const SharedPtr<Player>& player, QObject *parent = nullptr);
  void Init(QTcpSocket*);
  void SendCurrentTrackInfo();
  void SendMsg();

 private:
  PlaylistItemPtr current_item_;
  Playlist *playlist_;
  QTcpSocket *socket_;
  qint32 msg_type_;
  QByteArray msg_stream_;
  nw::remote::Message *msg_;
  long bytes_out_;
  std::string msg_string_;
  nw::remote::SongMetadata *song_;
  nw::remote::ResponseSongMetadata *response_song_;
  SharedPtr<Player> player_ ;
};

#endif
