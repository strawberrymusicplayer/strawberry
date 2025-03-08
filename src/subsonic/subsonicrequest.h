/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QHash>
#include <QMap>
#include <QMultiMap>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QJsonObject>

#include "core/song.h"
#include "subsonicbaserequest.h"

class QNetworkAccessManager;
class QNetworkReply;
class SubsonicService;
class SubsonicUrlHandler;
class NetworkTimeouts;

class SubsonicRequest : public SubsonicBaseRequest {
  Q_OBJECT

 public:
  explicit SubsonicRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, QObject *parent = nullptr);
  ~SubsonicRequest() override;

  void ReloadSettings();

  void GetAlbums();
  void Reset();

 private:
  struct Request {
    explicit Request() : offset(0), size(0) {}
    QString artist_id;
    QString album_id;
    QString song_id;
    int offset;
    int size;
    QString album_artist;
  };
  struct AlbumCoverRequest {
    QString artist_id;
    QString album_id;
    QString cover_id;
    QUrl url;
    QString filename;
  };

 Q_SIGNALS:
  void Results(const SongMap &songs, const QString &error);
  void UpdateStatus(const QString &text);
  void ProgressSetMaximum(const int max);
  void UpdateProgress(const int progress);

 private Q_SLOTS:
  void AlbumsReplyReceived(QNetworkReply *reply, const int offset_requested, const int size_requested);
  void AlbumSongsReplyReceived(QNetworkReply *reply, const QString &artist_id, const QString &album_id, const QString &album_artist);
  void AlbumCoverReceived(QNetworkReply *reply, const SubsonicRequest::AlbumCoverRequest &request);

 private:

  void AddAlbumsRequest(const int offset = 0, const int size = 500);
  void FlushAlbumsRequests();

  void AlbumsFinishCheck(const int offset = 0, const int size = 0, const int albums_received = 0);
  void SongsFinishCheck();

  void AddAlbumSongsRequest(const QString &artist_id, const QString &album_id, const QString &album_artist, const int offset = 0);
  void FlushAlbumSongsRequests();

  QString ParseSong(Song &song, const QJsonObject &json_object, const QString &artist_id_requested = QString(), const QString &album_id_requested = QString(), const QString &album_artist = QString(), const qint64 album_created = 0);

  void GetAlbumCovers();
  void AddAlbumCoverRequest(const Song &song);
  void FlushAlbumCoverRequests();
  void AlbumCoverFinishCheck();

  void FinishCheck();
  static void Warn(const QString &error, const QVariant &debug = QVariant());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  SubsonicService *service_;
  SubsonicUrlHandler *url_handler_;
  QNetworkAccessManager *network_;
  NetworkTimeouts *timeouts_;

  bool finished_;

  QQueue<Request> albums_requests_queue_;
  QQueue<Request> album_songs_requests_queue_;
  QQueue<AlbumCoverRequest> album_cover_requests_queue_;

  QHash<QString, Request> album_songs_requests_pending_;
  QMultiMap<QString, QString> album_covers_requests_sent_;
  QMultiMap<QString, QUrl> album_covers_retrieved_;

  int albums_requests_active_;

  int album_songs_requests_active_;
  int album_songs_requested_;
  int album_songs_received_;

  int album_covers_requests_active_;
  int album_covers_requested_;
  int album_covers_received_;

  SongMap songs_;
  QMap<QString, QUrl> cover_urls_;
  QStringList errors_;
  bool no_results_;
  QList<QNetworkReply*> replies_;
  QList<QNetworkReply*> album_cover_replies_;
};

#endif  // SUBSONICREQUEST_H
