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
#include <QHeaderView>
#include "playlist/playlistview.h"
#include "playlist/playlistdelegates.h"
#include "networkremoteoutgoingmsg.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlist.h"
#include "constants/timeconstants.h"
#include "constants/networkremoteconstants.h"

NetworkRemoteOutgoingMsg::NetworkRemoteOutgoingMsg(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : QObject(parent),
    player_(player),
    playlist_manager_(playlist_manager),
    bytes_out_(0),
    socket_(nullptr) {}

void NetworkRemoteOutgoingMsg::Init(QTcpSocket *socket) {
    socket_ = socket;
}

void NetworkRemoteOutgoingMsg::SetPlaylistView(QPointer<PlaylistView> playlist_view) {
    playlist_view_ = playlist_view;
    qLog(Debug) << "NetworkRemoteOutgoingMsg::SetPlaylistView, non-null:" << !playlist_view_.isNull();
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

        response_song_.setPlayerState(MapEngineState(player_->GetState()));
        response_song_.setSongMetadata(song_);

        const qint64 position_nanosec = player_->engine()->position_nanosec();
        qint64 length_nanosec = player_->engine()->length_nanosec();
        if (length_nanosec <= 0) {
            // Engine doesn't know yet (e.g. just-loaded track): fall back to the tag.
            length_nanosec = current_item_->EffectiveMetadata().length_nanosec();
        }

        response_song_.setPositionSeconds(position_nanosec > 0 ? static_cast<quint32>(position_nanosec / kNsecPerSec) : 0);
        response_song_.setLengthSeconds(length_nanosec > 0 ? static_cast<quint32>(length_nanosec / kNsecPerSec) : 0);

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

nw::remote::PlayerStateGadget::PlayerState NetworkRemoteOutgoingMsg::MapEngineState(EngineBase::State state) {
    switch (state) {
    case EngineBase::State::Empty:   return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_EMPTY;
    case EngineBase::State::Idle:    return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_IDLE;
    case EngineBase::State::Playing: return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_PLAYING;
    case EngineBase::State::Paused:  return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_PAUSED;
    case EngineBase::State::Error:   return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_ERROR;
    }
    return nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_UNSPECIFIED;
}

void NetworkRemoteOutgoingMsg::SendEngineState(EngineBase::State state) {
    msg_ =  nw::remote::Message();
    nw::remote::EngineStateChange state_change;
    switch (state) {
    case EngineBase::State::Playing: state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_PLAYING); break;
    case EngineBase::State::Paused:  state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_PAUSED);  break;
    case EngineBase::State::Idle:    state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_IDLE);    break;
    default:                         state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_EMPTY);   break;
    }
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_ENGINE_STATE_CHANGE);
    msg_.setEngineStateChange(state_change);

    SendMsg();
}

nw::remote::EngineStateChange NetworkRemoteOutgoingMsg::BuildEngineStateChange(EngineBase::State state) {
    nw::remote::EngineStateChange state_change;
    switch (state) {
    case EngineBase::State::Playing: state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_PLAYING); break;
    case EngineBase::State::Paused:  state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_PAUSED);  break;
    case EngineBase::State::Idle:    state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_IDLE);    break;
    default:                         state_change.setState(nw::remote::EngineStateGadget::EngineState::ENGINE_STATE_EMPTY);   break;
    }
    return state_change;
}

nw::remote::ResponseSongMetadata NetworkRemoteOutgoingMsg::BuildResponseSongMetadata() {
    nw::remote::ResponseSongMetadata response_song;
    PlaylistItemPtr current_item = player_->GetCurrentItem();

    if (current_item != nullptr) {
        nw::remote::SongMetadata song;
        song.setTitle(current_item->EffectiveMetadata().PrettyTitle());
        song.setAlbum(current_item->EffectiveMetadata().album());
        song.setArtist(current_item->EffectiveMetadata().artist());
        song.setAlbumartist(current_item->EffectiveMetadata().albumartist());
        song.setTrack(current_item->EffectiveMetadata().track());
        song.setStryear(current_item->EffectiveMetadata().PrettyYear());
        song.setGenre(current_item->EffectiveMetadata().genre());
        song.setPlaycount(current_item->EffectiveMetadata().playcount());
        song.setSonglength(current_item->EffectiveMetadata().PrettyLength());

        response_song.setPlayerState(MapEngineState(player_->GetState()));
        response_song.setSongMetadata(song);

        const qint64 position_nanosec = player_->engine()->position_nanosec();
        qint64 length_nanosec = player_->engine()->length_nanosec();
        if (length_nanosec <= 0) {
            length_nanosec = current_item->EffectiveMetadata().length_nanosec();
        }

        response_song.setPositionSeconds(position_nanosec > 0 ? static_cast<quint32>(position_nanosec / kNsecPerSec) : 0);
        response_song.setLengthSeconds(length_nanosec > 0 ? static_cast<quint32>(length_nanosec / kNsecPerSec) : 0);
    }
    else {
        response_song.setPlayerState(nw::remote::PlayerStateGadget::PlayerState::PLAYER_STATUS_UNSPECIFIED);
    }
    return response_song;
}

nw::remote::ResponsePlaylists NetworkRemoteOutgoingMsg::BuildResponsePlaylists() {
    QList<nw::remote::PlaylistInfo> playlist_infos;
    const int active_id = playlist_manager_->active_id();

    for (const int id : playlist_manager_->playlist_ids()) {
        Playlist *pl = playlist_manager_->playlist(id);
        nw::remote::PlaylistInfo info;
        info.setId_proto(static_cast<quint32>(id));
        info.setName(playlist_manager_->playlist_name(id));
        info.setItemCount(static_cast<quint32>(pl->rowCount()));
        info.setIsOpen(true);
        info.setIsPlaying(id == active_id);
        playlist_infos.append(info);
    }

    nw::remote::ResponsePlaylists response;
    response.setPlaylists(playlist_infos);
    return response;
}

void NetworkRemoteOutgoingMsg::SendInitialInfo() {
    msg_ = nw::remote::Message();

    nw::remote::ResponseInitialInfo initial_info;
    initial_info.setSongInfo(BuildResponseSongMetadata());
    initial_info.setPlaylists(BuildResponsePlaylists());
    initial_info.setEngineState(BuildEngineStateChange(player_->GetState()));

    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_INITIAL_INFO);
    msg_.setResponseInitialInfo(initial_info);
    SendMsg();
}

// Server-authoritative classification of which columns are numeric/
// measurement-like (right/center-align friendly) vs. free text. This mirrors
// Playlist::Column's own nature - the server knows the real type, so clients
// don't need to guess alignment from formatted cell content.
bool NetworkRemoteOutgoingMsg::IsNumericColumn(Playlist::Column column) {
    switch (column) {
    case Playlist::Column::Year:
    case Playlist::Column::OriginalYear:
    case Playlist::Column::Track:
    case Playlist::Column::Disc:
    case Playlist::Column::Length:
    case Playlist::Column::Samplerate:
    case Playlist::Column::Bitdepth:
    case Playlist::Column::Bitrate:
    case Playlist::Column::Filesize:
    case Playlist::Column::DateCreated:
    case Playlist::Column::DateModified:
    case Playlist::Column::PlayCount:
    case Playlist::Column::SkipCount:
    case Playlist::Column::LastPlayed:
    case Playlist::Column::Rating:
    case Playlist::Column::HasCUE:
    case Playlist::Column::EBUR128IntegratedLoudness:
    case Playlist::Column::EBUR128LoudnessRange:
    case Playlist::Column::BPM:
        return true;
    default:
        // Title, Artist, Album, Genre, Composer, Performer, Grouping,
        // Comment, URL, Filetype, Mood, InitialKey, Source, Moodbar, and
        // any future column default to text/left-aligned.
        return false;
    }
}

void NetworkRemoteOutgoingMsg::SendPlaylistSongs(const quint32 playlist_id, const quint32 upcoming_count) {
    msg_ = nw::remote::Message();

    Playlist *pl = playlist_manager_->playlist(static_cast<int>(playlist_id));
    nw::remote::ResponsePlaylistSongs response;
    response.setPlaylistId(playlist_id);

    if (pl && !playlist_view_.isNull()) {
        QHeaderView *header = playlist_view_->header();

        // Walk columns in visual (on-screen, drag-reordered) order, not
        // logical/enum order, so the client's column layout matches what's
        // actually displayed on the desktop.
        QList<int> visible_columns;
        QList<nw::remote::ColumnInfo> columns;
        for (int visual_pos = 0; visual_pos < header->count(); ++visual_pos) {
            const int col = header->logicalIndex(visual_pos);
            if (header->isSectionHidden(col)) continue;
            visible_columns.append(col);

            const Playlist::Column column_enum = static_cast<Playlist::Column>(col);
            nw::remote::ColumnInfo column_info;
            column_info.setName(Playlist::column_name(column_enum));
            column_info.setIsNumeric(IsNumericColumn(column_enum));
            columns.append(column_info);
        }
        response.setColumns(columns);

        const QLocale locale = QLocale::system();

        // rows[0] is the current/last-played row itself, not the first upcoming song.
        const int current_row = pl->current_row();
        const int start_row = (current_row >= 0) ? current_row : 0;
        const int total = pl->rowCount();
        const int end_row = std::min(total, start_row + 1 + static_cast<int>(upcoming_count));

        QList<nw::remote::PlaylistSongRow> rows;
        for (int row = start_row; row < end_row; ++row) {
            nw::remote::PlaylistSongRow song_row;
            QStringList values;
            for (int col : visible_columns) {
                const QVariant raw_value = pl->data(pl->index(row, col), Qt::DisplayRole);

                QString formatted;
                PlaylistDelegateBase *delegate = qobject_cast<PlaylistDelegateBase*>(playlist_view_->itemDelegateForColumn(col));
                if (delegate) {
                    formatted = delegate->displayText(raw_value, locale);
                }
                else {
                    formatted = raw_value.toString();
                }
                values.append(formatted);
            }
            song_row.setValues(values);
            song_row.setRowIndex(static_cast<quint32>(row));
            rows.append(song_row);
        }
        response.setRows(rows);
        response.setIsActive(playlist_manager_->active_id() == static_cast<int>(playlist_id));
    }
    else {
        response.setIsActive(false);
        if (playlist_view_.isNull()) {
            qLog(Warning) << "SendPlaylistSongs: playlist_view_ is null, cannot determine visible columns";
        }
    }

    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_PLAYLIST_SONGS);
    msg_.setResponsePlaylistSongs(response);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendPlaySongResponse(const bool accepted) {
    msg_ = nw::remote::Message();
    nw::remote::ResponsePlaySong response;
    response.setAccepted(accepted);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_PLAY_SONG);
    msg_.setResponsePlaySong(response);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendAddSongToPlaylistResponse(const bool accepted, const quint32 playlist_id) {
    msg_ = nw::remote::Message();
    nw::remote::ResponseAddSongToPlaylist response;
    response.setAccepted(accepted);
    response.setPlaylistId(playlist_id);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_ADD_SONG_TO_PLAYLIST);
    msg_.setResponseAddSongToPlaylist(response);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendRemoveSongFromPlaylistResponse(const bool accepted) {
    msg_ = nw::remote::Message();
    nw::remote::ResponseRemoveSongFromPlaylist response;
    response.setAccepted(accepted);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_REMOVE_SONG_FROM_PLAYLIST);
    msg_.setResponseRemoveSongFromPlaylist(response);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendPlaylistChanged(const quint32 playlist_id) {
    msg_ = nw::remote::Message();
    nw::remote::PlaylistChanged changed;
    changed.setPlaylistId(playlist_id);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_PLAYLIST_CHANGED);
    msg_.setPlaylistChanged(changed);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendPlaylistActivated(const quint32 playlist_id) {
    msg_ = nw::remote::Message();
    nw::remote::PlaylistActivated activated;
    activated.setPlaylistId(playlist_id);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_PLAYLIST_ACTIVATED);
    msg_.setPlaylistActivated(activated);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendMsg() {
    msg_.setVersion(NetworkRemoteConstants::kProtocolVersion);
    QProtobufSerializer serializer;
    QByteArray data = serializer.serialize(&msg_);

    if (serializer.lastError() != QAbstractProtobufSerializer::Error::None) {
        qLog(Warning) << "Failed to serialize message:" << serializer.lastErrorString();
        return;
    }

    // Prepend a 4-byte big-endian length header so the receiver can frame messages correctly.
    QByteArray framed_data;
    QDataStream len_stream(&framed_data, QIODevice::WriteOnly);
    len_stream.setByteOrder(QDataStream::BigEndian);
    len_stream << static_cast<quint32>(data.size());
    framed_data.append(data);
    bytes_out_ = framed_data.size();
    if (socket_ && socket_->isWritable()) {
        socket_->write(framed_data);
        qLog(Debug) << bytes_out_ << "bytes written to socket" << socket_->socketDescriptor();
    }
    else {
        qLog(Warning) << "Socket is not writable.";
    }
}

void NetworkRemoteOutgoingMsg::SendDisconnect(nw::remote::ReasonDisconnectGadget::ReasonDisconnect reason) {
    msg_ = nw::remote::Message();
    nw::remote::RequestDisconnect disconnect;
    disconnect.setReasonDisconnect(reason);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_DISCONNECT);
    msg_.setRequestDisconnect(disconnect);
    SendMsg();
}

void NetworkRemoteOutgoingMsg::SendConnectResponse(const bool accepted) {
    msg_ = nw::remote::Message();
    nw::remote::ResponseConnect response;
    response.setAccepted(accepted);
    msg_.setType(nw::remote::MsgTypeGadget::MsgType::MSG_TYPE_RESPONSE_CONNECT);
    msg_.setResponseConnect(response);
    SendMsg();
}