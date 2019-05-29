/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QList>
#include <QHash>
#include <QMap>
#include <QMultiMap>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/song.h"
#include "tidalbaserequest.h"

class NetworkAccessManager;
class TidalService;
class TidalUrlHandler;

class TidalRequest : public TidalBaseRequest {
  Q_OBJECT

 public:

  TidalRequest(TidalService *service, TidalUrlHandler *url_handler, NetworkAccessManager *network, QueryType type, QObject *parent);
  ~TidalRequest();

  void ReloadSettings();

  void Process();
  void NeedLogin() { need_login_ = true; }
  void Search(const int search_id, const QString &search_text);

 signals:
  void Login();
  void Login(const QString &username, const QString &password, const QString &token);
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void Results(SongList songs);
  void SearchResults(int id, SongList songs);
  void ErrorSignal(QString message);
  void ErrorSignal(int id, QString message);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int max);
  void UpdateProgress(int max);
  void StreamURLFinished(const QUrl original_url, const QUrl url, const Song::FileType, QString error = QString());

 public slots:
  void GetArtists(const int offset = 0);
  void GetAlbums(const int offset = 0);
  void GetSongs(const int offset = 0);

 private slots:
  void LoginComplete(bool success, QString error = QString());
  void ArtistsReceived(QNetworkReply *reply, const int limit_requested = 0, const int offset_requested = 0);
  void AlbumsReceived(QNetworkReply *reply, const int artist_id = 0, const int limit_requested = 0, const int offset_requested = 0);
  void SongsReceived(QNetworkReply *reply, const int album_id);
  void AlbumCoverReceived(QNetworkReply *reply, const int album_id, const QUrl url);

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  const bool IsQuery() { return (type_ == QueryType_Artists || type_ == QueryType_Albums || type_ == QueryType_Songs); }
  const bool IsSearch() { return (type_ == QueryType_SearchArtists || type_ == QueryType_SearchAlbums || type_ == QueryType_SearchSongs); }

  void SendSearch();
  void ArtistsSearch(const int offset = 0);
  void AlbumsSearch(const int offset = 0);
  void SongsSearch(const int offset = 0);
  void ArtistsFinishCheck(const int limit = 0, const int offset = 0, const int artists_received = 0);
  void AlbumsFinishCheck(const int artist_id, const int limit = 0, const int offset = 0, const int albums_total = 0, const int albums_received = 0);
  void GetArtistAlbums(const int artist_id, const int offset = 0);
  void GetAlbumSongs(const int album_id);
  int ParseSong(Song &song, const int album_id_requested, const QJsonValue &value, QString album_artist = QString());
  void GetAlbumCovers();
  void GetAlbumCover(Song &song);
  void FinishCheck();
  QString LoginError(QString error, QVariant debug = QVariant());
  QString Error(QString error, QVariant debug = QVariant());

  static const char *kResourcesUrl;

  TidalService *service_;
  TidalUrlHandler *url_handler_;
  NetworkAccessManager *network_;

  QueryType type_;

  int search_id_;
  QString search_text_;
  QList<int> artist_albums_queue_;
  QList<int> artist_albums_requests_;
  QHash<int, QString> album_songs_requests_;
  QMultiMap<int, Song*> album_covers_requests_;
  int artists_total_;
  int artists_chunk_requested_;
  int artists_chunk_received_;
  int artists_received_;
  int artist_albums_chunk_requested_;
  int artist_albums_chunk_received_;
  int artist_albums_received_;
  int album_songs_received_;
  int album_covers_requested_;
  int album_covers_received_;
  SongList songs_;
  QString errors_;
  bool need_login_;
  bool no_results_;
  QList<QNetworkReply*> replies_;

};

#endif  // TIDALREQUEST_H
