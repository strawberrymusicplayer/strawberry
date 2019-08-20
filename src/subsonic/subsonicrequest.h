/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SUBSONICREQUEST_H
#define SUBSONICREQUEST_H

#include "config.h"

#include <stdbool.h>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QList>
#include <QHash>
#include <QMultiMap>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonObject>

#include "core/song.h"
#include "subsonicbaserequest.h"

class Application;
class NetworkAccessManager;
class SubsonicService;
class SubsonicUrlHandler;

class SubsonicRequest : public SubsonicBaseRequest {
  Q_OBJECT

 public:

  SubsonicRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, Application *app, QObject *parent);
  ~SubsonicRequest();

  void ReloadSettings();

  void GetAlbums();
  void Reset();

 signals:
  void Results(const SongList &songs, const QString &error);
  void UpdateStatus(const QString &text);
  void ProgressSetMaximum(const int max);
  void UpdateProgress(const int max);

 private slots:
  void AlbumsReplyReceived(QNetworkReply *reply, const int offset_requested);
  void AlbumSongsReplyReceived(QNetworkReply *reply, const qint64 artist_id, const qint64 album_id, const QString &album_artist);
  void AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename);

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  struct Request {
    qint64 artist_id = 0;
    qint64 album_id = 0;
    qint64 song_id = 0;
    int offset = 0;
    int size = 0;
    QString album_artist;
  };
  struct AlbumCoverRequest {
    qint64 artist_id = 0;
    QString album_id = 0;
    QUrl url;
    QString filename;
  };

  void AddAlbumsRequest(const int offset = 0, const int size = 0);
  void FlushAlbumsRequests();

  void AlbumsFinishCheck(const int offset = 0, const int albums_received = 0);
  void SongsFinishCheck();

  void AddAlbumSongsRequest(const qint64 artist_id, const qint64 album_id, const QString &album_artist, const int offset = 0);
  void FlushAlbumSongsRequests();

  int ParseSong(Song &song, const QJsonObject &json_obj, const qint64 artist_id_requested = 0, const qint64 album_id_requested = 0, const QString &album_artist = QString());

  void GetAlbumCovers();
  void AddAlbumCoverRequest(Song &song);
  void FlushAlbumCoverRequests();
  void AlbumCoverFinishCheck();

  void FinishCheck();
  void Warn(const QString &error, const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant());

  static const int kMaxConcurrentAlbumsRequests;
  static const int kMaxConcurrentArtistAlbumsRequests;
  static const int kMaxConcurrentAlbumSongsRequests;
  static const int kMaxConcurrentAlbumCoverRequests;

  SubsonicService *service_;
  SubsonicUrlHandler *url_handler_;
  Application *app_;
  std::unique_ptr<QNetworkAccessManager> network_;

  bool finished_;

  QQueue<Request> albums_requests_queue_;
  QQueue<Request> album_songs_requests_queue_;
  QQueue<AlbumCoverRequest> album_cover_requests_queue_;

  QHash<int, Request> album_songs_requests_pending_;
  QMultiMap<QString, Song*> album_covers_requests_sent_;

  int albums_requests_active_;

  int album_songs_requests_active_;
  int album_songs_requested_;
  int album_songs_received_;

  int album_covers_requests_active_;
  int album_covers_requested_;
  int album_covers_received_;

  SongList songs_;
  QStringList errors_;
  bool no_results_;
  QList<QNetworkReply*> replies_;
  QList<QNetworkReply*> album_cover_replies_;

};

#endif  // SUBSONICREQUEST_H
