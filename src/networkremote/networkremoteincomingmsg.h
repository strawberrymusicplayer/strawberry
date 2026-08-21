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

#ifndef NETWORKREMOTEINCOMINGMSG_H
#define NETWORKREMOTEINCOMINGMSG_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include "networkremoteprotothelper.h"
#include "networkremote/RemoteMessages.qpb.h"

class QTcpSocket;

class NetworkRemoteIncomingMsg : public QObject{
    Q_OBJECT
public:
    explicit NetworkRemoteIncomingMsg(QObject *parent = nullptr);
    ~NetworkRemoteIncomingMsg();
    void Init(QTcpSocket* socket);
    nwr_types::MsgType GetMsgType();
    quint32 GetMsgVersion();
    QString GetClientName();
    nwr::RequestPlaylistSongs GetRequestPlaylistSongs();
    nwr::RequestPlaySong GetRequestPlaySong();
    nwr::RequestAddSongToPlaylist GetRequestAddSongToPlaylist();
    nwr::RequestRemoveSongFromPlaylist GetRequestRemoveSongFromPlaylist();

private Q_SLOTS:
    void ReadyRead();

Q_SIGNALS:
    void InMsgParsed();

private:
    nwr::Message msg_;
    nwr_types::MsgType msg_type_ = nwr_types::MsgType::MSG_TYPE_UNSPECIFIED;
    QTcpSocket *socket_;
    QByteArray msg_stream_;
    void SetMsgType();
};

#endif