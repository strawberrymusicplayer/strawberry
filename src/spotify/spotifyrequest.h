/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYREQUEST_H
#define SPOTIFYREQUEST_H

#include "config.h"

#include <QMap>
#include <QMultiMap>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QJsonObject>
#include <QTimer>
#include <QScopedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "spotifybaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class SpotifyService;

class SpotifyRequest : public SpotifyBaseRequest {
  Q_OBJECT

 public:
  explicit SpotifyRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, const Type type, QObject *parent);
  ~SpotifyRequest() override;

  void ReloadSettings();

  void Process();
  void Search(const int query_id, const QString &search_text);

 private:
  struct Artist {
    QString artist_id;
    QString artist;
  };
  struct Album {
    QString album_id;
    QString album;
    QUrl cover_url;
  };
  struct Request {
    Request() : offset(0), limit(0) {}
    int offset;
    int limit;
  };
  struct ArtistAlbumsRequest {
    ArtistAlbumsRequest() : offset(0), limit(0) {}
    Artist artist;
    int offset;
    int limit;
  };
  struct AlbumSongsRequest {
    AlbumSongsRequest() : offset(0), limit(0) {}
    Artist artist;
    Album album;
    int offset;
    int limit;
  };
  struct AlbumCoverRequest {
    QString artist_id;
    QString album_id;
    QUrl url;
    QString filename;
  };

 Q_SIGNALS:
  void Results(int id, SongMap songs, QString error);
  void UpdateStatus(int id, QString text);
  void ProgressSetMaximum(int id, int max);
  void UpdateProgress(int id, int max);
  void StreamURLFinished(QUrl original_url, QUrl url, Song::FileType, QString error = QString());

 private Q_SLOTS:
  void FlushRequests();

  void ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);

  void AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);
  void AlbumsReceived(QNetworkReply *reply, const SpotifyRequest::Artist &artist_artist, const int limit_requested, const int offset_requested);

  void SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);
  void SongsReceived(QNetworkReply *reply, const SpotifyRequest::Artist &artist, const SpotifyRequest::Album &album, const int limit_requested, const int offset_requested);

  void ArtistAlbumsReplyReceived(QNetworkReply *reply, const SpotifyRequest::Artist &artist, const int offset_requested);
  void AlbumSongsReplyReceived(QNetworkReply *reply, const SpotifyRequest::Artist &artist, const SpotifyRequest::Album &album, const int offset_requested);
  void AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename);

 private:
  void StartRequests();

  bool IsQuery() const { return (type_ == Type::FavouriteArtists || type_ == Type::FavouriteAlbums || type_ == Type::FavouriteSongs); }
  bool IsSearch() const { return (type_ == Type::SearchArtists || type_ == Type::SearchAlbums || type_ == Type::SearchSongs); }

  void GetArtists();
  void GetAlbums();
  void GetSongs();

  void ArtistsSearch();
  void AlbumsSearch();
  void SongsSearch();

  void AddArtistsRequest(const int offset = 0, const int limit = 0);
  void AddArtistsSearchRequest(const int offset = 0);
  void FlushArtistsRequests();
  void AddAlbumsRequest(const int offset = 0, const int limit = 0);
  void AddAlbumsSearchRequest(const int offset = 0);
  void FlushAlbumsRequests();
  void AddSongsRequest(const int offset = 0, const int limit = 0);
  void AddSongsSearchRequest(const int offset = 0);
  void FlushSongsRequests();

  void ArtistsFinishCheck(const int limit, const int offset, const int artists_received);
  void AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received);
  void SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received);

  void AddArtistAlbumsRequest(const Artist &artist, const int offset = 0);
  void FlushArtistAlbumsRequests();

  void AddAlbumSongsRequest(const Artist &artist, const Album &album, const int offset = 0);
  void FlushAlbumSongsRequests();

  void ParseSong(Song &song, const QJsonObject &json_obj, const Artist &album_artist, const Album &album);

  void GetAlbumCoversCheck();
  void GetAlbumCovers();
  void AddAlbumCoverRequest(const Song &song);
  void FlushAlbumCoverRequests();
  void AlbumCoverFinishCheck();

  int GetProgress(const int count, const int total);
  void FinishCheck();
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;
  void Warn(const QString &error_message, const QVariant &debug);

 private:
  const SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_flush_requests_;

  const Type type_;
  bool fetchalbums_;
  QString coversize_;

  int query_id_;
  QString search_text_;

  bool finished_;

  QQueue<Request> artists_requests_queue_;
  QQueue<Request> albums_requests_queue_;
  QQueue<Request> songs_requests_queue_;

  QQueue<ArtistAlbumsRequest> artist_albums_requests_queue_;
  QQueue<AlbumSongsRequest> album_songs_requests_queue_;
  QQueue<AlbumCoverRequest> album_cover_requests_queue_;

  QMap<QString, ArtistAlbumsRequest> artist_albums_requests_pending_;
  QMap<QString, AlbumSongsRequest> album_songs_requests_pending_;
  QMultiMap<QString, QString> album_covers_requests_sent_;

  int artists_requests_total_;
  int artists_requests_active_;
  int artists_requests_received_;
  int artists_total_;
  int artists_received_;

  int albums_requests_total_;
  int albums_requests_active_;
  int albums_requests_received_;
  int albums_total_;
  int albums_received_;

  int songs_requests_total_;
  int songs_requests_active_;
  int songs_requests_received_;
  int songs_total_;
  int songs_received_;

  int artist_albums_requests_total_;
  int artist_albums_requests_active_;
  int artist_albums_requests_received_;
  int artist_albums_total_;
  int artist_albums_received_;

  int album_songs_requests_active_;
  int album_songs_requests_received_;
  int album_songs_requests_total_;
  int album_songs_total_;
  int album_songs_received_;

  int album_covers_requests_total_;
  int album_covers_requests_active_;
  int album_covers_requests_received_;

  SongMap songs_;
  bool no_results_;
  QString error_;
};

using SpotifyRequestPtr = QScopedPointer<SpotifyRequest, QScopedPointerDeleteLater>;

#endif  // SPOTIFYREQUEST_H
