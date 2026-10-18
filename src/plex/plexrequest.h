/*
 * Strawberry Music Player
 * Copyright 2025-2026, Strawberry Music Player contributors
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

#ifndef PLEXREQUEST_H
#define PLEXREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QQueue>
#include <QHash>
#include <QUrl>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "core/song.h"
#include "plexbaserequest.h"

class QNetworkReply;
class PlexService;
class PlexUrlHandler;

class PlexRequest : public PlexBaseRequest {
  Q_OBJECT

 public:
  explicit PlexRequest(PlexService *service, const SharedPtr<NetworkAccessManager> network, PlexUrlHandler *url_handler, const SongMap &existing_songs = SongMap(), const QStringList &rejected_song_ids = QStringList(), const qint64 updated_since = 0, QObject *parent = nullptr);
  ~PlexRequest() override;

  void GetLibrarySections();

  // The cover URLs and the cache filenames, these don't need a request, so they can be tested.
  // Returns the image ID from the thumb path of a Plex cover URL, which is used in the cache filename, or an empty string if it's not a Plex cover URL.
  static QString CoverImageId(const QUrl &cover_url);
  // Returns the cover URL for the thumb path on the server, without a query, so it never includes an access token.
  static QUrl CoverUrl(const QUrl &server_url, const QString &thumb);
  // Updates a saved cover URL to the current server, for a cover which was not downloaded, or a downloaded cover which is not in the cache anymore.
  static QUrl CoverUrlForServer(const QUrl &cover_url, const QUrl &server_url);
  // Returns the cache directory for the covers of the server, each server has its own directory, since different servers can have the same thumb paths.
  static QString CoverCacheDir(const QString &server_machine_identifier, const QUrl &server_url);

  qint64 newest_updated_at() const { return newest_updated_at_; }
  bool incremental() const { return incremental_; }
  QStringList rejected_song_ids() const { return QStringList(rejected_song_ids_.cbegin(), rejected_song_ids_.cend()); }

 Q_SIGNALS:
  void Results(const SongMap &songs, const QString &error);
  void UpdateStatus(const QString &text);
  void ProgressSetMaximum(const int max);
  void UpdateProgress(const int progress);

 private:
  void HandleLibrarySectionsReply(QNetworkReply *reply);
  void GetSectionSongs(const QString &section_key, const int offset = 0);
  void HandleSectionSongsReply(QNetworkReply *reply, const QString &section_key, const int offset_requested);
  void GetSectionCount(const QString &section_key);
  void HandleSectionCountReply(QNetworkReply *reply);
  void ParseSong(const QJsonObject &object_metadata);
  void RejectSong(const QString &song_id, const QString &reason, const QJsonObject &object_metadata);

  void SongsFinishCheck();

  struct AlbumCoverRequest {
    QString image_id;
    QUrl url;
    QString filename;
  };
  void GetAlbumCovers();
  void AddAlbumCoverRequest(const Song &song);
  void FlushAlbumCoverRequests();
  void AlbumCoverReceived(QNetworkReply *reply, const AlbumCoverRequest &request);

  void FinishCheck();
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  PlexUrlHandler *url_handler_;

  bool finished_;
  bool incremental_;
  qint64 updated_since_;
  qint64 newest_updated_at_;
  int server_total_songs_;

  int songs_requests_active_;
  int songs_total_;
  int songs_received_;

  bool album_covers_started_;
  QQueue<AlbumCoverRequest> album_cover_requests_queue_;
  QHash<QString, QStringList> album_covers_requests_sent_;
  int album_covers_requests_active_;
  int album_covers_requested_;
  int album_covers_received_;

  SongMap songs_;
  QSet<QString> rejected_song_ids_;  // Songs the server counts, but which could not be parsed.
  QStringList errors_;
  QList<QNetworkReply*> replies_;
};

#endif  // PLEXREQUEST_H
