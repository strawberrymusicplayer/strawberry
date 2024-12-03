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

#ifndef OUTGOINGDATACREATOR_H
#define OUTGOINGDATACREATOR_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QTcpSocket>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "playlist/playlistsequence.h"
#include "networkremotemessages.qpb.h"
#include "covermanager/albumcoverloaderresult.h"

class Database;
class Player;
class PlaylistManager;
class PlaylistBackend;
class Playlist;
class NetworkRemoteClient;

class OutgoingDataCreator : public QObject {
  Q_OBJECT

 public:
  explicit OutgoingDataCreator(const SharedPtr<Database> database,
                               const SharedPtr<Player> player,
                               const SharedPtr<PlaylistManager> playlist_manager,
                               const SharedPtr<PlaylistBackend> playlist_backend,
                               QObject *parent = nullptr);

  ~OutgoingDataCreator();

  void SetClients(QList<NetworkRemoteClient*> *clients);
  void SetRemoteRootFiles(const QString &files_root_folder) {
    files_root_folder_ = files_root_folder;
  }
  void SetMusicExtensions(const QStringList &files_music_extensions) {
    files_music_extensions_ = files_music_extensions;
  }
  void SetAllowDownloads(bool allow_downloads) {
    allow_downloads_ = allow_downloads;
  }

  static networkremote::SongMetadata PbSongMetadataFromSong(const int index, const Song &song, const QImage &image_cover_art = QImage());

 public Q_SLOTS:
  void SendInfo();
  void SendKeepAlive();
  void SendAllPlaylists();
  void SendAllActivePlaylists();
  void SendFirstData(const bool send_playlist_songs);
  void SendPlaylistSongs(const int id);
  void PlaylistChanged(Playlist *playlist);
  void VolumeChanged(const uint volume);
  void PlaylistAdded(const int id, const QString &name, bool favorite);
  void PlaylistDeleted(const int id);
  void PlaylistClosed(const int id);
  void PlaylistRenamed(const int id, const QString &new_name);
  void ActiveChanged(Playlist *playlist);
  void CurrentSongChanged(const Song &song, const AlbumCoverLoaderResult &result);
  void SendSongMetadata();
  void StateChanged(const EngineBase::State state);
  void SendRepeatMode(const PlaylistSequence::RepeatMode mode);
  void SendShuffleMode(const PlaylistSequence::ShuffleMode mode);
  void UpdateTrackPosition();
  void DisconnectAllClients();
  void SendCollection(NetworkRemoteClient *client);
  void SendListFiles(QString relative_path, NetworkRemoteClient *client);

 private:
  void SendDataToClients(networkremote::Message *msg);
  networkremote::EngineStateGadget::EngineState GetEngineState();

 private:
  const SharedPtr<Database> database_;
  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<PlaylistBackend> playlist_backend_;
  QList<NetworkRemoteClient*> *clients_;
  Song current_song_;
  AlbumCoverLoaderResult albumcoverloader_result_;
  QImage current_image_;
  EngineBase::State last_state_;
  QTimer *keep_alive_timer_;
  QTimer *track_position_timer_;
  int keep_alive_timeout_;
  int last_track_position_;
  QString files_root_folder_;
  QStringList files_music_extensions_;
  bool allow_downloads_;
};

#endif  // OUTGOINGDATACREATOR_H
