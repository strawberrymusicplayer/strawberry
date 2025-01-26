/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TIDALREQUEST_H
#define TIDALREQUEST_H

#include "config.h"

#include <QHash>
#include <QMap>
#include <QMultiMap>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QScopedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"

#include "tidalbaserequest.h"

class QNetworkReply;
class QTimer;
class NetworkAccessManager;
class TidalService;
class TidalUrlHandler;

class TidalRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  explicit TidalRequest(TidalService *service, TidalUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent);

  void ReloadSettings();

  void Process();
  void Search(const int query_id, const QString &search_text);

 private:
  struct Artist {
    QString artist_id;
    QString artist;
  };
  struct Album {
    Album() : album_explicit(false) {}
    QString album_id;
    QString album;
    QUrl cover_url;
    bool album_explicit;
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
  void LoginSuccess();
  void LoginFailure(const QString &failure_reason);
  void Results(const int id, const SongMap &songs = SongMap(), const QString &error = QString());
  void UpdateStatus(const int id, const QString &text);
  void UpdateProgress(const int id, const int max);
  void StreamURLFinished(const QUrl &media_url, const QUrl &url, const Song::FileType filetype, const QString &error = QString());

 private Q_SLOTS:
  void ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);

  void AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);
  void AlbumsReceived(QNetworkReply *reply, const TidalRequest::Artist &artist_requested, const int limit_requested, const int offset_requested);

  void SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested);
  void SongsReceived(QNetworkReply *reply, const TidalRequest::Artist &artist, const TidalRequest::Album &album, const int limit_requested, const int offset_requested);

  void ArtistAlbumsReplyReceived(QNetworkReply *reply, const TidalRequest::Artist &artist, const int offset_requested);
  void AlbumSongsReplyReceived(QNetworkReply *reply, const TidalRequest::Artist &artist, const TidalRequest::Album &album, const int offset_requested);
  void AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename);

 private:
  bool IsQuery() const { return (query_type_ == Type::FavouriteArtists || query_type_ == Type::FavouriteAlbums || query_type_ == Type::FavouriteSongs); }
  bool IsSearch() const { return (query_type_ == Type::SearchArtists || query_type_ == Type::SearchAlbums || query_type_ == Type::SearchSongs); }

  void StartRequests();
  void FlushRequests();

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

  void ArtistsFinishCheck(const int limit = 0, const int offset = 0, const int artists_received = 0);
  void AlbumsFinishCheck(const Artist &artist, const int limit = 0, const int offset = 0, const int albums_total = 0, const int albums_received = 0);
  void SongsFinishCheck(const Artist &artist, const Album &album, const int limit = 0, const int offset = 0, const int songs_total = 0, const int songs_received = 0);

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
  static void Warn(const QString &error_message, const QVariant &debug_output = QVariant());
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;

  TidalService *service_;
  TidalUrlHandler *url_handler_;
  SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_flush_requests_;

  const Type query_type_;
  const bool fetchalbums_;
  const QString coversize_;

  int query_id_;
  QString search_text_;

  bool finished_;
  QString error_;

  QQueue<Request> artists_requests_queue_;
  QQueue<Request> albums_requests_queue_;
  QQueue<Request> songs_requests_queue_;

  QQueue<ArtistAlbumsRequest> artist_albums_requests_queue_;
  QQueue<AlbumSongsRequest> album_songs_requests_queue_;
  QQueue<AlbumCoverRequest> album_cover_requests_queue_;

  QHash<QString, ArtistAlbumsRequest> artist_albums_requests_pending_;
  QHash<QString, AlbumSongsRequest> album_songs_requests_pending_;
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
};

using TidalRequestPtr = QScopedPointer<TidalRequest, QScopedPointerDeleteLater>;

#endif  // TIDALREQUEST_H
