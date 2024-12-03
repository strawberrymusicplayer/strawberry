/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Andreas Muttscheller <asfa194@gmail.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QList>

#include "includes/shared_ptr.h"
#include "includes/scoped_ptr.h"

class QMimeData;
class QHostAddress;
class QTcpServer;
class QTcpSocket;

class Database;
class Player;
class CollectionBackend;
class PlaylistManager;
class PlaylistBackend;
class CurrentAlbumCoverLoader;
class AudioScrobbler;
class IncomingDataParser;
class OutgoingDataCreator;
class NetworkRemoteClient;

class NetworkRemote : public QObject {
  Q_OBJECT

 public:
  explicit NetworkRemote(const SharedPtr<Database> database,
                         const SharedPtr<Player> player,
                         const SharedPtr<CollectionBackend> collection_backend,
                         const SharedPtr<PlaylistManager> playlist_manager,
                         const SharedPtr<PlaylistBackend> playlist_backend,
                         const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                         const SharedPtr<AudioScrobbler> scrobbler,
                         QObject *parent = nullptr);

  ~NetworkRemote();

 Q_SIGNALS:
  void AddToPlaylistSignal(QMimeData *data);
  void SetCurrentPlaylist(const int id);

 public Q_SLOTS:
  void SetupServer();
  void StartServer();
  void ReloadSettings();
  void AcceptConnection();

 private:
  const SharedPtr<Database> database_;
  const SharedPtr<Player> player_;
  const SharedPtr<CollectionBackend> collection_backend_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<PlaylistBackend> playlist_backend_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  const SharedPtr<AudioScrobbler> scrobbler_;

  ScopedPtr<QTcpServer> server_;
  ScopedPtr<QTcpServer> server_ipv6_;
  ScopedPtr<IncomingDataParser> incoming_data_parser_;
  ScopedPtr<OutgoingDataCreator> outgoing_data_creator_;

  bool enabled_;
  quint16 port_;
  bool allow_public_access_;
  bool signals_connected_;

  QList<NetworkRemoteClient*> clients_;

  void StopServer();
  void CreateRemoteClient(QTcpSocket *client_socket);
  bool IpIsPrivate(const QHostAddress &address);
};

#endif  // NETWORKREMOTE_H
