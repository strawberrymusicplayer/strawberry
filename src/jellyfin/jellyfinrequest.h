/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#ifndef JELLYFINREQUEST_H
#define JELLYFINREQUEST_H

#include "config.h"

#include <QHash>
#include <QQueue>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/song.h"

#include "jellyfinbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class NetworkTimeouts;
class JellyfinService;

class JellyfinRequest : public JellyfinBaseRequest {
  Q_OBJECT

 public:
  explicit JellyfinRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent);

  void Process();
  void Search(const int query_id, const QString &search_text);
  void Abort();

 Q_SIGNALS:
  void Results(const int id, const SongMap &songs = SongMap(), const QString &error = QString());
  // Songs received so far, emitted while the request continues, so they can be shown before it has finished.
  // The songs emitted with Results can differ, for example by pointing to a downloaded album cover.
  void SongsAvailable(const int id, const SongMap &songs);
  void UpdateStatus(const int id, const QString &text);
  void UpdateProgress(const int id, const int max);

 private:
  struct AlbumCoverRequest {
    QString image_id;
    QUrl url;
    QString filename;
  };

  bool IsSearch() const {
    return query_type_ == Type::SearchArtists || query_type_ == Type::SearchAlbums || query_type_ == Type::SearchSongs;
  }

  // Artists and albums are expanded into their tracks, since only songs can be played.
  bool IsParentQuery() const {
    return query_type_ == Type::FavouriteArtists || query_type_ == Type::FavouriteAlbums || query_type_ == Type::SearchArtists || query_type_ == Type::SearchAlbums;
  }

  bool IsArtistQuery() const {
    return query_type_ == Type::FavouriteArtists || query_type_ == Type::SearchArtists;
  }

  // The catalog of the streaming view the results are for, none for searches.
  int Catalog() const;

  void StartRequests();
  void AddRequest(const int offset = 0);
  void FlushRequests();

  void ReplyReceived(QNetworkReply *reply, const int offset_requested);
  bool RetryPage(const int offset);
  void Unauthorized(QNetworkReply *reply);

  struct TracksRequest {
    QString parent_id;
    int offset;
    int retries;
  };

  void AddTracksRequest(const QString &parent_id, const int offset = 0, const int retries = 0);
  void FlushTracksRequests();
  void TracksReplyReceived(QNetworkReply *reply, const TracksRequest &request);

  bool ParseItem(Song &song, const QJsonObject &json_object);
  bool ParseAudio(Song &song, const QJsonObject &json_object);

  void GetAlbumCovers();
  void AddAlbumCoverRequest(const Song &song);
  void FlushAlbumCoverRequests();
  void AlbumCoverReceived(QNetworkReply *reply, const AlbumCoverRequest &request);
  void AlbumCoverFinishCheck();

  QString IncludeItemTypes() const;
  QString RessourcePath() const;
  QString CreateImageUrl(const QString &item_id, const QString &image_tag = QString()) const;

  int GetProgress(const int count, const int total);
  void FinishCheck();

  static void Warn(const QString &error, const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  NetworkTimeouts *timeouts_;

  const Type query_type_;

  int query_id_;
  QString search_text_;

  bool finished_;
  QStringList errors_;

  QQueue<int> requests_queue_;
  QHash<int, int> page_retries_;
  QSet<int> pages_queued_;
  QSet<int> pages_scheduled_;
  int requests_active_;
  int items_total_;
  int items_received_;
  bool paging_complete_;
  bool unauthorized_;

  QQueue<TracksRequest> tracks_requests_queue_;
  QSet<QString> tracks_requests_sent_;
  int tracks_requests_active_;

  QQueue<AlbumCoverRequest> album_cover_requests_queue_;
  QHash<QString, QStringList> album_covers_requests_sent_;
  int album_covers_requests_active_;
  int album_covers_requested_;
  int album_covers_received_;

  SongMap songs_;
  QHash<QString, QString> parent_names_;  // The names of the artists and albums the songs were loaded for, by ID.
};

#endif  // JELLYFINREQUEST_H
