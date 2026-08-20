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
#include "networkremotesettings.h"
#include "core/player.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlist.h"

NetworkRemoteClient::NetworkRemoteClient(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : QObject(parent),
    player_(player),
    playlist_manager_ (playlist_manager),
    incoming_msg_(new NetworkRemoteIncomingMsg(this)),
    outgoing_msg_(new NetworkRemoteOutgoingMsg(player, playlist_manager, this)) {
    QObject::connect(this, &NetworkRemoteClient::RequestPlay, player_.get(), [this]() { player_->Play(); });
    QObject::connect(this, &NetworkRemoteClient::RequestPause, player_.get(), &Player::Pause);
    QObject::connect(this, &NetworkRemoteClient::RequestNext, player_.get(), &Player::Next);
    QObject::connect(this, &NetworkRemoteClient::RequestPrevious, player_.get(), &Player::Previous);
    QObject::connect(this, &NetworkRemoteClient::RequestStop, player_.get(), [this]() { player_->Stop(); });
    QObject::connect(this, &NetworkRemoteClient::RequestPlaylistSongs, this, &NetworkRemoteClient::HandleRequestPlaylistSongs);
    QObject::connect(this, &NetworkRemoteClient::RequestPlaySong, this, &NetworkRemoteClient::HandleRequestPlaySong);
    QObject::connect(this, &NetworkRemoteClient::RequestAddSongToPlaylist, this, &NetworkRemoteClient::HandleRequestAddSongToPlaylist);
    QObject::connect(this, &NetworkRemoteClient::RequestRemoveSongFromPlaylist, this, &NetworkRemoteClient::HandleRequestRemoveSongFromPlaylist);
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

void NetworkRemoteClient::SetPlaylistView(QPointer<PlaylistView> playlist_view) {
    outgoing_msg_->SetPlaylistView(playlist_view);
}

void NetworkRemoteClient::SendPlaylistChanged(quint32 playlist_id) {
    outgoing_msg_->SendPlaylistChanged(playlist_id);
}

void NetworkRemoteClient::SendPlaylistActivated(quint32 playlist_id) {
    outgoing_msg_->SendPlaylistActivated(playlist_id);
}

void NetworkRemoteClient::HandleRequestPlaylistSongs(const quint32 playlist_id, const quint32 upcoming_count) {
    outgoing_msg_->SendPlaylistSongs(playlist_id, upcoming_count);
}

// Plays a specific row within a specific playlist - the same SetActiveToCurrent()
// + PlayAt() sequence MainWindow::PlayIndex() uses for a double-click on the
// desktop, just driven by an explicit row index from the network instead of a
// QModelIndex from a UI click.
void NetworkRemoteClient::HandleRequestPlaySong(const quint32 playlist_id, const quint32 row_index) {
    playlist_manager_->SetCurrentOrOpen(static_cast<int>(playlist_id));
    playlist_manager_->SetActiveToCurrent();
    player_->PlayAt(static_cast<int>(row_index), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Never, true);
    outgoing_msg_->SendPlaySongResponse(true);
}

// Adds the currently-playing song to an existing playlist, or to a brand new
// one if new_playlist_name is non-empty. Per the design decision this only
// targets open playlists - a closed target has no live Playlist object to
// mutate, so it's rejected rather than silently opened.
void NetworkRemoteClient::HandleRequestAddSongToPlaylist(const quint32 target_playlist_id, const QString new_playlist_name) {
    int resolved_id = static_cast<int>(target_playlist_id);

    if (!new_playlist_name.isEmpty()) {
        int captured_id = -1;
        QMetaObject::Connection conn = QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistAdded, this,
                                                        [&captured_id](int id, const QString &, bool) { captured_id = id; });
        playlist_manager_->New(new_playlist_name);
        QObject::disconnect(conn);
        resolved_id = captured_id;
    }

    Playlist *pl = playlist_manager_->playlist(resolved_id);
    bool accepted = false;
    if (pl) {
        PlaylistItemPtr current_item = player_->GetCurrentItem();
        if (current_item) {
            SongList songs;
            songs.append(current_item->EffectiveMetadata());
            pl->InsertSongs(songs, -1, false, false, false, true);
            accepted = true;
        }
    }
    const PlaylistRejectReason reject_reason = accepted ? PlaylistRejectReason::PLAYLIST_REJECT_NONE : PlaylistRejectReason::PLAYLIST_REJECT_INVALID_REQUEST;
    outgoing_msg_->SendAddSongToPlaylistResponse(accepted, accepted ? static_cast<quint32>(resolved_id) : 0, reject_reason);
}

// Removes a single row from an open playlist. row_index must come from a
// recent ResponsePlaylistSongs - the client is expected to only offer removal
// on current/upcoming rows, not stale history rows, to avoid an index that no
// longer matches the playlist's actual contents.
void NetworkRemoteClient::HandleRequestRemoveSongFromPlaylist(const quint32 playlist_id, const quint32 row_index) {
    Playlist *pl = playlist_manager_->playlist(static_cast<int>(playlist_id));
    bool accepted = false;
    if (pl && pl->has_item_at(static_cast<int>(row_index))) {
        pl->RemoveItemsWithoutUndo(QList<int>{static_cast<int>(row_index)});
        accepted = true;
    }
    const PlaylistRejectReason reject_reason = accepted ? PlaylistRejectReason::PLAYLIST_REJECT_NONE : PlaylistRejectReason::PLAYLIST_REJECT_INVALID_REQUEST;
    outgoing_msg_->SendRemoveSongFromPlaylistResponse(accepted, reject_reason);
}

void NetworkRemoteClient::ProcessIncoming() {
    const quint32 client_version = incoming_msg_->GetMsgVersion();
    if (client_version < kMinSupportedVersion) {
        qLog(Warning) << "Rejecting client with protocol version" << client_version
                      << "- minimum supported is" << kMinSupportedVersion;
        outgoing_msg_->SendDisconnect(ReasonDisconnect::REASON_DISCONNECT_VERSION_MISMATCH);
        Q_EMIT ClientIsLeaving();
        return;   // don't process the message
    }

    const MsgType msg_type = incoming_msg_->GetMsgType();

    if (!handshake_complete_) {
        if (msg_type != MsgType::MSG_TYPE_REQUEST_CONNECT) {
            qLog(Warning) << "Client sent message type" << static_cast<int>(msg_type) << "before handshake - disconnecting";
            outgoing_msg_->SendDisconnect(ReasonDisconnect::REASON_DISCONNECT_NO_HANDSHAKE);
            Q_EMIT ClientIsLeaving();
            return;
        }
        handshake_complete_ = true;
        const bool auth_enabled = !NetworkRemoteSettings::CurrentToken().isEmpty();
        qLog(Debug) << "Handshake from client:" << incoming_msg_->GetClientName() << "protocol version" << client_version;
        outgoing_msg_->SendConnectResponse(true, auth_enabled);
        return;
    }

    switch (msg_type) {
    case MsgType::MSG_TYPE_REQUEST_SONG_INFO:
        outgoing_msg_->SendCurrentTrackInfo();
        break;
    case MsgType::MSG_TYPE_REQUEST_PLAY:
        Q_EMIT RequestPlay();
        outgoing_msg_->SendCurrentTrackInfo();
        break;
    case MsgType::MSG_TYPE_REQUEST_NEXT:
        Q_EMIT RequestNext();
        outgoing_msg_->SendCurrentTrackInfo();
        break;
    case MsgType::MSG_TYPE_REQUEST_PREVIOUS:
        Q_EMIT RequestPrevious();
        outgoing_msg_->SendCurrentTrackInfo();
        break;
    case MsgType::MSG_TYPE_REQUEST_PAUSE:
        Q_EMIT RequestPause();
        break;
    case MsgType::MSG_TYPE_REQUEST_STOP:
        Q_EMIT RequestStop();
        break;
    case MsgType::MSG_TYPE_REQUEST_FINISH:
        Q_EMIT ClientIsLeaving();
        break;
    case MsgType::MSG_TYPE_DISCONNECT:
        Q_EMIT ClientIsLeaving();
        break;
    case MsgType::MSG_TYPE_REQUEST_CONNECT:
        qLog(Warning) << "Duplicate handshake ignored";
        break;
    case MsgType::MSG_TYPE_REQUEST_INITIAL_INFO:
        outgoing_msg_->SendInitialInfo();
        break;
    case MsgType::MSG_TYPE_REQUEST_PLAYLIST_SONGS: {
        const nwr::RequestPlaylistSongs request = incoming_msg_->GetRequestPlaylistSongs();
        Q_EMIT RequestPlaylistSongs(request.playlistId(), request.upcomingCount());
        break;
    }
    case MsgType::MSG_TYPE_REQUEST_PLAY_SONG: {
        const nwr::RequestPlaySong request = incoming_msg_->GetRequestPlaySong();
        Q_EMIT RequestPlaySong(request.playlistId(), request.rowIndex());
        break;
    }
    case MsgType::MSG_TYPE_REQUEST_ADD_SONG_TO_PLAYLIST: {
        const nwr::RequestAddSongToPlaylist request = incoming_msg_->GetRequestAddSongToPlaylist();
        const QString expected_token = NetworkRemoteSettings::CurrentToken();
        if (!expected_token.isEmpty()) {
            const PlaylistRejectReason reject_reason =
                request.token().isEmpty()
                    ? PlaylistRejectReason::PLAYLIST_REJECT_TOKEN_REQUIRED
                    : PlaylistRejectReason::PLAYLIST_REJECT_TOKEN_MISMATCH;
            if (request.token() != expected_token) {
                qLog(Warning) << "Rejected RequestAddSongToPlaylist: token check failed";
                outgoing_msg_->SendAddSongToPlaylistResponse(false, 0, reject_reason);
                break;
            }
        }
        Q_EMIT RequestAddSongToPlaylist(request.targetPlaylistId(), request.newPlaylistName());
        break;
    }
    case MsgType::MSG_TYPE_REQUEST_REMOVE_SONG_FROM_PLAYLIST: {
        const nwr::RequestRemoveSongFromPlaylist request = incoming_msg_->GetRequestRemoveSongFromPlaylist();
        const QString expected_token = NetworkRemoteSettings::CurrentToken();
        if (!expected_token.isEmpty()) {
            const PlaylistRejectReason reject_reason =
                request.token().isEmpty()
                    ? PlaylistRejectReason::PLAYLIST_REJECT_TOKEN_REQUIRED
                    : PlaylistRejectReason::PLAYLIST_REJECT_TOKEN_MISMATCH;
            if (request.token() != expected_token) {
                qLog(Warning) << "Rejected RequestRemoveSongFromPlaylist: token check failed";
                outgoing_msg_->SendRemoveSongFromPlaylistResponse(false, reject_reason);
                break;
            }
        }
        Q_EMIT RequestRemoveSongFromPlaylist(request.playlistId(), request.rowIndex());
        break;
    }
    default:
        qLog(Debug) << "Unknown message type";
        outgoing_msg_->SendDisconnect(ReasonDisconnect::REASON_DISCONNECT_UNKNOWN_MSGTYPE);
        Q_EMIT ClientIsLeaving();
        break;
    }
}

void NetworkRemoteClient::SendEngineState(EngineBase::State state) {
    outgoing_msg_->SendEngineState(state);
}

void NetworkRemoteClient::SendDisconnect(ReasonDisconnect reason) {
    outgoing_msg_->SendDisconnect(reason);
}