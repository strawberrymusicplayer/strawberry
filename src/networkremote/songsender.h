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

#ifndef SONGSENDER_H
#define SONGSENDER_H

#include <QObject>
#include <QMap>
#include <QQueue>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "networkremotemessages.qpb.h"
#include "transcoder/transcoder.h"

class Player;
class CollectionBackend;
class PlaylistManager;
class NetworkRemoteClient;
class Transcoder;

class DownloadItem {
 public:
  explicit DownloadItem(const Song &song, const int song_number, const int song_count)
      : song_(song), song_number_(song_number), song_count_(song_count) {}

  Song song_;
  int song_number_;
  int song_count_;
};

class SongSender : public QObject {
  Q_OBJECT

 public:
  explicit SongSender(const SharedPtr<Player> player,
                      const SharedPtr<CollectionBackend> collection_backend,
                      const SharedPtr<PlaylistManager> playlist_manager,
                      NetworkRemoteClient *client,
                      QObject *parent = nullptr);

  ~SongSender();

 public Q_SLOTS:
  void SendSongs(const networkremote::RequestDownloadSongs &request);
  void ResponseSongOffer(bool accepted);

 private Q_SLOTS:
  void TranscodeJobComplete(const QString &input, const QString &output, const bool success);
  void StartTransfer();

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<CollectionBackend> collection_backend_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  NetworkRemoteClient *client_;

  TranscoderPreset transcoder_preset_;
  Transcoder *transcoder_;
  bool transcode_lossless_files_;

  QQueue<DownloadItem> download_queue_;
  QMap<QString, QString> transcoder_map_;
  int total_transcode_;

  void SendSingleSong(const DownloadItem &download_item);
  void SendAlbum(const Song &song);
  void SendPlaylist(const networkremote::RequestDownloadSongs &request);
  void SendUrls(const networkremote::RequestDownloadSongs &request);
  void OfferNextSong();
  void SendTotalFileSize();
  void TranscodeLosslessFiles();
  void SendTranscoderStatus();
};

#endif  // SONGSENDER_H
