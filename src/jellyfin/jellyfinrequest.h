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
#include <QList>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QScopedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/networktimeouts.h"

#include "jellyfinbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class JellyfinService;

class JellyfinRequest : public JellyfinBaseRequest {
  Q_OBJECT

 public:
  explicit JellyfinRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent);

  void Process();
  void Search(const int query_id, const QString &search_text);

 Q_SIGNALS:
  void Results(const int id, const SongMap &songs = SongMap(), const QString &error = QString());
  void UpdateStatus(const int id, const QString &text);
  void UpdateProgress(const int id, const int max);

 private:
  struct AlbumCoverRequest {
    QString album_id;
    QUrl url;
    QString filename;
  };

  bool IsSearch() const {
    return query_type_ == Type::SearchArtists || query_type_ == Type::SearchAlbums || query_type_ == Type::SearchSongs;
  }

  void StartRequests();
  void AddRequest(const int offset = 0);
  void FlushRequests();

  void ReplyReceived(QNetworkReply *reply, const int offset_requested);
  bool RetryPage(const int offset);

  bool ParseItem(Song &song, const QJsonObject &json_object);
  bool ParseAudio(Song &song, const QJsonObject &json_object);
  bool ParseAlbum(Song &song, const QJsonObject &json_object);
  bool ParseArtist(Song &song, const QJsonObject &json_object);

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

  JellyfinService *service_;
  const SharedPtr<NetworkAccessManager> network_;
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
  int requests_received_;
  int items_total_;
  int items_received_;
  bool paging_complete_;
  bool page_cap_hit_;

  QQueue<AlbumCoverRequest> album_cover_requests_queue_;
  QHash<QString, QStringList> album_covers_requests_sent_;
  int album_covers_requests_active_;
  int album_covers_requested_;
  int album_covers_received_;

  SongMap songs_;
};

using JellyfinRequestPtr = QScopedPointer<JellyfinRequest, QScopedPointerDeleteLater>;

#endif  // JELLYFINREQUEST_H