/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, Andreas Muttscheller <asfa194@gmail.com>
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

#ifndef NETWORKREMOTECLIENT_H
#define NETWORKREMOTECLIENT_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QAbstractSocket>

#include "networkremotemessages.qpb.h"
#include "songsender.h"

class QTcpSocket;

class NetworkRemoteClient : public QObject {
  Q_OBJECT

 public:
  explicit NetworkRemoteClient(const SharedPtr<Player> player,
                               const SharedPtr<CollectionBackend> collection_backend,
                               const SharedPtr<PlaylistManager> playlist_manager,
                               QTcpSocket *client,
                               QObject *parent = nullptr);

  ~NetworkRemoteClient();

  void SendData(networkremote::Message *msg);
  QAbstractSocket::SocketState State() const;
  void setDownloader(const bool downloader);
  bool isDownloader() const { return downloader_; }
  void DisconnectClient(const networkremote::ReasonDisconnectGadget::ReasonDisconnect reason);

  SongSender *song_sender() const { return song_sender_; }
  const QString &files_root_folder() const { return files_root_folder_; }
  const QStringList &files_music_extensions() const { return files_music_extensions_; }
  bool allow_downloads() const { return allow_downloads_; }

 public Q_SLOTS:
  void ReadyRead();
  void IncomingData();

 Q_SIGNALS:
  void Parse(const networkremote::Message &msg);

 private:
  void ParseMessage(const QByteArray &data);
  void SendDataToClient(networkremote::Message *msg);

 private:
  const SharedPtr<Player> player_;
  QTcpSocket *socket_;

  bool use_auth_code_;
  int auth_code_;
  bool authenticated_;
  bool allow_downloads_;
  bool downloader_;

  bool reading_protobuf_;
  quint32 expected_length_;
  QByteArray buffer_;
  SongSender *song_sender_;

  QString files_root_folder_;
  QStringList files_music_extensions_;
};

#endif  // NETWORKREMOTECLIENT_H
