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

#ifndef NETWORKREMOTECLIENT_H
#define NETWORKREMOTECLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QPointer>
#include "networkremoteincomingmsg.h"
#include "networkremoteoutgoingmsg.h"
#include "includes/shared_ptr.h"
#include "networkremotehelper.h"

class Player;
class PlaylistManager;
class PlaylistView;

class NetworkRemoteClient : public QObject {
  Q_OBJECT
 public:
  explicit NetworkRemoteClient(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent = nullptr);
  ~NetworkRemoteClient();
  void Init(QTcpSocket *);
  QTcpSocket *GetSocket();
  void ProcessIncoming();
  void SendEngineState(EngineBase::State state);
  void SendDisconnect(nwr_types::ReasonDisconnect reason);
  void SetPlaylistView(QPointer<PlaylistView> playlist_view);
  void SendPlaylistChanged(quint32 playlist_id);
  void SendPlaylistActivated(quint32 playlist_id);
  void SendPlaylistSongs(quint32 playlist_id, quint32 upcoming_count);
  void SendPlaylistAdvanced(quint32 playlist_id, quint32 new_current_row);
  void SendAuthStatusChanged(bool auth_enabled);

 Q_SIGNALS:
  void ReceiveMsg();
  void PrepareResponse();
  void ClientIsLeaving();
  void RequestPlay();
  void RequestPause();
  void RequestNext();
  void RequestPrevious();
  void RequestStop();
  void RequestPlaylistSongs(quint32 playlist_id, quint32 upcoming_count);
  void RequestPlaySong(quint32 playlist_id, quint32 row_index);
  void RequestAddSongToPlaylist(quint32 target_playlist_id, QString new_playlist_name);
  void RequestRemoveSongFromPlaylist(quint32 playlist_id, quint32 row_index);

 private:
  void HandleRequestPlaylistSongs(quint32 playlist_id, quint32 upcoming_count);
  void HandleRequestPlaySong(quint32 playlist_id, quint32 row_index);
  void HandleRequestAddSongToPlaylist(quint32 target_playlist_id, QString new_playlist_name);
  void HandleRequestRemoveSongFromPlaylist(quint32 playlist_id, quint32 row_index);

  // Checks token against the currently configured server token. Empty
  // configured token means auth is disabled and this always returns
  // true. On failure, sets *reject_reason and increments
  // failed_token_attempts_; on success, resets it to 0. Comparison is
  // performed byte-by-byte over the full length of both strings rather
  // than short-circuiting on the first mismatch, to avoid leaking how
  // many leading characters matched via response timing.
  bool TokenAccepted(const QString &token, nwr_types::PlaylistRejectReason *reject_reason);

  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  QTcpSocket *socket_;
  NetworkRemoteIncomingMsg *incoming_msg_;
  NetworkRemoteOutgoingMsg *outgoing_msg_;
  bool handshake_complete_ = false;
  int failed_token_attempts_ = 0;
};

#endif