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

#ifndef INCOMINGDATAPARSER_H
#define INCOMINGDATAPARSER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "constants/behavioursettings.h"
#include "core/player.h"
#include "networkremoteclient.h"
#include "networkremotemessages.qpb.h"
#include "playlist/playlistsequence.h"

class PlaylistManager;
class AudioScrobbler;

class IncomingDataParser : public QObject {
  Q_OBJECT

 public:
  explicit IncomingDataParser(const SharedPtr<Player> player,
                              const SharedPtr<PlaylistManager> playlist_manager,
                              const SharedPtr<AudioScrobbler> scrobbler,
                              QObject *parent = nullptr);

  ~IncomingDataParser();

  bool close_connection() const;

  void SetRemoteRootFiles(const QString &files_root_folder);

 public Q_SLOTS:
  void Parse(const networkremote::Message &msg);
  void ReloadSettings();

 Q_SIGNALS:
  void SendInfo();
  void SendFirstData(const bool send_playlist_songs);
  void SendAllPlaylists();
  void SendAllActivePlaylists();
  void SendPlaylistSongs(const int id);
  void New(const QString &name, const SongList &songs = SongList(), const QString &special_type = QString());
  void Open(const int id);
  void Clear(const int id);
  void Close(const int id);
  void Rename(const int id, const QString &new_playlist_name);
  void Favorite(const int id, const bool favorite);
  void GetLyrics();
  void Love();

  void Play();
  void PlayPause();
  void Pause();
  void Stop(const bool stop_after = false);
  void StopAfterCurrent();
  void Next();
  void Previous();
  void SetVolume(const uint volume);
  void PlayAt(const int index, const bool pause, const quint64 offset_nanosec, EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform);
  void Enqueue(const int id, const int i);
  void SetActivePlaylist(const int id);
  void ShuffleCurrent();
  void SetRepeatMode(const PlaylistSequence::RepeatMode repeat_mode);
  void SetShuffleMode(const PlaylistSequence::ShuffleMode shuffle_mode);
  void InsertUrls(const int id, const QList<QUrl> &urls, const int pos = -1, const bool play_now = false, const bool enqueue = false);
  void InsertSongs(const int id, const SongList &songs, const int pos, const bool play_now, const bool enqueue);
  void RemoveSongs(const int id, const QList<int> &indices);
  void SeekTo(const quint64 seconds);
  void SendCollection(NetworkRemoteClient *client);
  void RateCurrentSong(const float rating);

  void SendListFiles(const QString &path, NetworkRemoteClient *client);
  void AddToPlaylistSignal(QMimeData *data);
  void SetCurrentPlaylist(const int id);

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<AudioScrobbler> scrobbler_;

  bool close_connection_;
  BehaviourSettings::PlaylistAddBehaviour doubleclick_playlist_addmode_;
  QString files_root_folder_;

  void ClientConnect(const networkremote::Message &msg, NetworkRemoteClient *client);
  Song SongFromPbSongMetadata(const networkremote::SongMetadata &pb_song_metadata) const;

  void ParseGetPlaylistSongs(const networkremote::Message &msg);
  void ParseChangeSong(const networkremote::Message &msg);
  void ParseSetRepeatMode(const networkremote::Repeat &repeat);
  void ParseSetShuffleMode(const networkremote::Shuffle &shuffle);
  void ParseInsertUrls(const networkremote::Message &msg);
  void ParseRemoveSongs(const networkremote::Message &msg);
  void ParseSendPlaylists(const networkremote::Message &msg);
  void ParseOpenPlaylist(const networkremote::Message &msg);
  void ParseClosePlaylist(const networkremote::Message &msg);
  void ParseUpdatePlaylist(const networkremote::Message &msg);
  void ParseRateSong(const networkremote::Message &msg);
  void ParseAppendFilesToPlaylist(const networkremote::Message &msg);
};

#endif  // INCOMINGDATAPARSER_H
