/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OPENTIDALCOVERPROVIDER_H
#define OPENTIDALCOVERPROVIDER_H

#include "config.h"

#include <QQueue>
#include <QVariant>
#include <QString>
#include <QDateTime>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"

class QTimer;
class QNetworkReply;
class NetworkAccessManager;
class OAuthenticator;

class OpenTidalCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit OpenTidalCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

 private:
  class ArtworkRequest {
   public:
    explicit ArtworkRequest(const QString &_artwork_id) : artwork_id(_artwork_id) {}
    QString artwork_id;
  };
  using ArtworkRequestPtr = SharedPtr<ArtworkRequest>;

  class AlbumCoverRequest {
   public:
    explicit AlbumCoverRequest(const QString &_album_id, const QString &_album_title) : album_id(_album_id), album_title(_album_title) {}
    QString album_id;
    QString album_title;
    QList<ArtworkRequestPtr> artwork_requests;
  };
  using AlbumCoverRequestPtr = SharedPtr<AlbumCoverRequest>;

  class SearchRequest {
   public:
    explicit SearchRequest(const int _id, const QString &_artist, const QString &_album, const QString &_title) : id(_id), artist(_artist), album(_album), title(_title), finished(false) {}
    int id;
    QString artist;
    QString album;
    QString title;
    QList<AlbumCoverRequestPtr> albumcover_requests;
    CoverProviderSearchResults results;
    bool finished;
  };
  using SearchRequestPtr = SharedPtr<SearchRequest>;

  class QueuedSearchRequest {
   public:
    explicit QueuedSearchRequest(SearchRequestPtr _search) : search(_search) {}
    SearchRequestPtr search;
  };
  using QueuedSearchRequestPtr = SharedPtr<QueuedSearchRequest>;

  class QueuedAlbumCoverRequest {
   public:
    explicit QueuedAlbumCoverRequest(SearchRequestPtr _search, AlbumCoverRequestPtr _albumcover) : search(_search), albumcover(_albumcover) {}
    SearchRequestPtr search;
    AlbumCoverRequestPtr albumcover;
  };
  using QueuedAlbumCoverRequestPtr = SharedPtr<QueuedAlbumCoverRequest>;

  class QueuedArtworkRequest {
   public:
    explicit QueuedArtworkRequest(SearchRequestPtr _search, AlbumCoverRequestPtr _albumcover, ArtworkRequestPtr _artwork) : search(_search), albumcover(_albumcover), artwork(_artwork) {}
    SearchRequestPtr search;
    AlbumCoverRequestPtr albumcover;
    ArtworkRequestPtr artwork;
  };
  using QueuedArtworkRequestPtr = SharedPtr<QueuedArtworkRequest>;

 private:
  void LoginCheck();
  void Login();
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void SendSearchRequest(SearchRequestPtr request);
  void AddAlbumCoverRequest(SearchRequestPtr search_request, const QString &album_id, const QString &album_title);
  void SendAlbumCoverRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request);
  void AddArtworkRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, const QString &artwork_id);
  void SendArtworkRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, ArtworkRequestPtr artwork_request);
  void FinishAllSearches();
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private Q_SLOTS:
  void OAuthFinished(const bool success, const QString &error = QString());
  void FlushRequests();
  void HandleSearchReply(QNetworkReply *reply, OpenTidalCoverProvider::SearchRequestPtr search_request);
  void HandleAlbumCoverReply(QNetworkReply *reply, SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request);
  void HandleArtworkReply(QNetworkReply *reply, SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, ArtworkRequestPtr artwork_request);

 private:
  OAuthenticator *oauth_;
  QTimer *timer_flush_requests_;
  bool login_in_progress_;
  QDateTime last_login_attempt_;
  QQueue<QueuedSearchRequestPtr> search_requests_queue_;
  QQueue<QueuedAlbumCoverRequestPtr> albumcover_requests_queue_;
  QQueue<QueuedArtworkRequestPtr> artwork_requests_queue_;
};

#endif  // OPENTIDALCOVERPROVIDER_H
