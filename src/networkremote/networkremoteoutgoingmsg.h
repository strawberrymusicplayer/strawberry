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
#include <QPointer>
#include "playlist/playlist.h"
#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "networkremoteprotothelper.h"
#include "networkremote/RemoteMessages.qpb.h"

class Playlist;
class PlaylistManager;
class PlaylistView;
class Player;
class QTcpSocket;

class NetworkRemoteOutgoingMsg : public QObject{
    Q_OBJECT
public:
    explicit NetworkRemoteOutgoingMsg(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent = nullptr);
    void Init(QTcpSocket *);
    void SendCurrentTrackInfo();
    void SendEngineState(EngineBase::State state);
    void SendInitialInfo();
    void SendMsg();
    void SendDisconnect(nwr_types::ReasonDisconnect reason);
    void SendConnectResponse(const bool accepted, const bool auth_enabled);
    void SendPlaylistSongs(const quint32 playlist_id, const quint32 upcoming_count);
    void SendPlaySongResponse(const bool accepted);
    void SendAddSongToPlaylistResponse(const bool accepted,
        const quint32 playlist_id,
        const nwr_types::PlaylistRejectReason reject_reason = nwr_types::PlaylistRejectReason::PLAYLIST_REJECT_NONE);
    void SendRemoveSongFromPlaylistResponse(const bool accepted,
        const nwr_types::PlaylistRejectReason reject_reason = nwr_types::PlaylistRejectReason::PLAYLIST_REJECT_NONE);
    void SendPlaylistChanged(const quint32 playlist_id);
    void SendPlaylistActivated(const quint32 playlist_id);
    void SetPlaylistView(QPointer<PlaylistView> playlist_view);
    bool IsNumericColumn(Playlist::Column column);

private:
    static nwr_types::PlayerState MapEngineState(EngineBase::State state);
    static nwr::EngineStateChange BuildEngineStateChange(EngineBase::State state);
    nwr::ResponseSongMetadata BuildResponseSongMetadata();
    nwr::ResponsePlaylists BuildResponsePlaylists();

    SharedPtr<Player> player_ ;
    SharedPtr<PlaylistManager> playlist_manager_;
    QPointer<PlaylistView> playlist_view_;
    qint64 bytes_out_;
    QTcpSocket *socket_;
    QByteArray msg_stream_;
    std::string msg_string_;
    nwr::Message msg_;
};

#endif