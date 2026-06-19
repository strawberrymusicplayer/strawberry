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

#ifndef OPENTIDALREQUEST_H
#define OPENTIDALREQUEST_H

#include "config.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QJsonObject>
#include <QScopedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"

#include "opentidalbaserequest.h"

class QNetworkReply;
class QTimer;
class NetworkAccessManager;
class OpenTidalService;
class OpenTidalUrlHandler;

class OpenTidalRequest : public OpenTidalBaseRequest {
  Q_OBJECT

 public:
  explicit OpenTidalRequest(OpenTidalService *service, OpenTidalUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent);

  void Process();
  void Search(const int query_id, const QString &search_text);

 Q_SIGNALS:
  void Results(const int id, const SongMap &songs = SongMap(), const QString &error = QString());
  void UpdateStatus(const int id, const QString &text);
  void UpdateProgress(const int id, const int max);

 private:
  // The different kinds of requests this engine issues to the TIDAL Open API.
  // Every query type is eventually resolved into album expansions, since album items carry the track and volume numbers.
  enum class RequestType {
    DiscoverTracks,   // Search/collection tracks, parsed directly into songs.
    DiscoverAlbums,   // Search/collection albums, collecting album ids to expand.
    DiscoverArtists,  // Search/collection artists, collecting artist ids.
    ArtistAlbums,     // Albums belonging to an artist, collecting album ids to expand.
    AlbumExpand       // A single album with its items, parsed into songs.
  };

  struct Request {
    Request() : type(RequestType::DiscoverTracks) {}
    Request(const RequestType _type, const QUrl &_url) : type(_type), url(_url) {}
    RequestType type;
    QUrl url;
  };

 private:
  bool IsQuery() const { return (query_type_ == Type::FavouriteArtists || query_type_ == Type::FavouriteAlbums || query_type_ == Type::FavouriteSongs); }
  bool IsSearch() const { return (query_type_ == Type::SearchArtists || query_type_ == Type::SearchAlbums || query_type_ == Type::SearchSongs); }

  QUrl ApiUrl(const QString &path, const QStringList &includes) const;
  QUrl NextUrl(const QJsonObject &json_object) const;

  void AddRequest(const RequestType type, const QUrl &url);
  void StartRequests();
  void FlushRequests();

  void ReplyReceived(QNetworkReply *reply, const RequestType type);

  // JSON:API helpers.
  static QHash<QString, QJsonObject> ParseIncluded(const QJsonObject &json_object);
  static QString IncludeKey(const QString &type, const QString &id);
  static QString RelationshipId(const QJsonObject &resource, const QString &relationship);
  static QStringList RelationshipIds(const QJsonObject &resource, const QString &relationship);
  static qint64 ParseDuration(const QString &str);

  void HandleDiscoverTracks(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included);
  void HandleDiscoverAlbums(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included);
  void HandleDiscoverArtists(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included);
  void HandleArtistAlbums(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included);
  void HandleAlbumExpand(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included);

  void AddAlbumExpandRequest(const QString &album_id);
  void AddArtistAlbumsRequest(const QString &artist_id);

  Song ParseTrack(const QJsonObject &track, const QString &album_id, const QString &album_title, const QString &album_artist, const QUrl &cover_url, const int volume_number, const int track_number);
  QUrl ParseCoverUrl(const QString &album_id, const QHash<QString, QJsonObject> &included);

  void FinishCheck();
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;

 private:
  OpenTidalService *service_;
  OpenTidalUrlHandler *url_handler_;
  QTimer *timer_flush_requests_;

  const Type query_type_;
  const QString coversize_;

  int query_id_;
  QString search_text_;

  bool finished_;
  QString error_;

  QQueue<Request> requests_queue_;
  int requests_active_;
  int requests_total_;
  int requests_received_;

  QSet<QString> albums_seen_;
  QSet<QString> artists_seen_;

  SongMap songs_;
};

using OpenTidalRequestPtr = QScopedPointer<OpenTidalRequest, QScopedPointerDeleteLater>;

#endif  // OPENTIDALREQUEST_H
