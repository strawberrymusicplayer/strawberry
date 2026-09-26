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

#ifndef JELLYFINSERVICE_H
#define JELLYFINSERVICE_H

#include "config.h"

#include <QObject>
#include <QVariant>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDateTime>
#include <QQueue>
#include <QScopedPointer>
#include <QSslError>

#include "includes/shared_ptr.h"
#include "constants/jellyfinsettings.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "collection/collectionmodel.h"

class QTimer;
class QNetworkReply;

class TaskManager;
class Database;
class NetworkAccessManager;
class UrlHandlers;
class AlbumCoverLoader;
class CollectionBackend;
class CollectionFilter;
class JellyfinRequest;
class JellyfinScrobbleRequest;
class JellyfinUrlHandler;

using JellyfinRequestPtr = QScopedPointer<JellyfinRequest, QScopedPointerDeleteLater>;

class JellyfinService : public StreamingService {
  Q_OBJECT

 public:
  explicit JellyfinService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<NetworkAccessManager> network,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent = nullptr);

  ~JellyfinService() override;

  static const Song::Source kSource;
  static const char *kClientName;
  static const char *kApiVersion;

  void ReloadSettings() override;
  void Exit() override;

  bool authenticated() const override { return !access_token_.isEmpty(); }

  QUrl server_url() const { return server_url_; }
  QString username() const { return username_; }
  QString password() const { return password_; }
  QString access_token() const { return access_token_; }
  QString user_id() const { return user_id_; }
  bool http2() const { return http2_; }
  bool verify_certificate() const { return verify_certificate_; }
  bool download_album_covers() const { return download_album_covers_; }
  bool server_side_scrobbling() const { return server_side_scrobbling_; }

  QUrl GetStreamUrl(const QString &song_id) const;
  void Scrobble(const QString &song_id, const bool submission, const QDateTime &time);
  void ReportPlaybackProgress(const QString &song_id, const QDateTime &time);

  QString CreateAuthorizationHeader(const bool with_token = false) const;

  SharedPtr<CollectionBackend> artists_collection_backend() override { return artists_collection_backend_; }
  SharedPtr<CollectionBackend> albums_collection_backend() override { return albums_collection_backend_; }
  SharedPtr<CollectionBackend> songs_collection_backend() override { return songs_collection_backend_; }

  CollectionModel *artists_collection_model() override { return artists_collection_model_; }
  CollectionModel *albums_collection_model() override { return albums_collection_model_; }
  CollectionModel *songs_collection_model() override { return songs_collection_model_; }

  CollectionFilter *artists_collection_filter_model() override { return artists_collection_model_->filter(); }
  CollectionFilter *albums_collection_filter_model() override { return albums_collection_model_->filter(); }
  CollectionFilter *songs_collection_filter_model() override { return songs_collection_model_->filter(); }

  int Search(const QString &search_text, const SearchType type) override;
  void CancelSearch() override;

 public Q_SLOTS:
  void SendPing();
  void SendPingWithCredentials(QUrl url, const QString &username, const QString &password);
  void Reauthenticate();
  void Catalog401();

  void GetArtists() override;
  void GetAlbums() override;
  void GetSongs() override;

  void ResetArtistsRequest() override;
  void ResetAlbumsRequest() override;
  void ResetSongsRequest() override;

 Q_SIGNALS:
  void ScrobbleError(const QString &error);

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void HandleAuthReply(QNetworkReply *reply, const QUrl &server_url, const QString &username, const QString &password);
  void HandleTestAuthReply(QNetworkReply *reply);
  void ExitReceived();

  void ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void ArtistsUpdateStatusReceived(const int id, const QString &text);
  void ArtistsUpdateProgressReceived(const int id, const int progress);

  void AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void AlbumsUpdateStatusReceived(const int id, const QString &text);
  void AlbumsUpdateProgressReceived(const int id, const int progress);

  void SongsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void SongsUpdateStatusReceived(const int id, const QString &text);
  void SongsUpdateProgressReceived(const int id, const int progress);

  void SearchResultsReceived(const int id, const SongMap &songs, const QString &error);
  void SearchUpdateStatus(const int id, const QString &text);
  void SearchUpdateProgress(const int id, const int progress);

  void StartSearch();

 private:
  struct AuthReply {
    QString error;
    QString access_token;
    QString user_id;
  };

  static QUrl NormalizedServerUrl(const QUrl &url);
  static QUrl AuthUrl(const QUrl &server_url);
  bool IsConfiguredLogin(const QUrl &server_url, const QString &username, const QString &password) const;
  void Login(const bool test);
  QNetworkReply *CreateAuthenticateRequest(const QUrl &url, const QString &username, const QString &password);
  SharedPtr<JellyfinScrobbleRequest> ScrobbleRequest();
  AuthReply ParseAuthReply(QNetworkReply *reply) const;
  void SetAuth(const QString &access_token, const QString &user_id, const QUrl &server_url, const QString &username);
  void AuthError(const QString &error, const bool test);
  void EmitTestResult(const QString &error);
  void FlushPendingScrobbles();

  // A playback report that arrived while the service was
  // not yet authenticated. It is buffered and sent once a login completes.
  struct PendingScrobbleRequest {
    enum class Type : int {
      Start,
      Progress,
      Stopped,
    } type;
    QString song_id;
    QDateTime time;
  };

  const SharedPtr<NetworkAccessManager> network_;
  const SharedPtr<Database> database_;
  const SharedPtr<TaskManager> task_manager_;

  JellyfinUrlHandler *url_handler_;

  SharedPtr<CollectionBackend> artists_collection_backend_;
  SharedPtr<CollectionBackend> albums_collection_backend_;
  SharedPtr<CollectionBackend> songs_collection_backend_;

  CollectionModel *artists_collection_model_;
  CollectionModel *albums_collection_model_;
  CollectionModel *songs_collection_model_;

  JellyfinRequestPtr artists_request_;
  JellyfinRequestPtr albums_request_;
  JellyfinRequestPtr songs_request_;
  JellyfinRequestPtr search_request_;
  SharedPtr<JellyfinScrobbleRequest> scrobble_request_;

  QTimer *timer_search_delay_;
  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  QString search_text_;
  SearchType pending_search_type_;
  int search_id_;

  QUrl server_url_;
  QString username_;
  QString password_;
  QString access_token_;
  QString user_id_;
  QUrl authenticated_server_url_;  // Normalized server URL and username the access token belongs to.
  QString authenticated_username_;
  bool http2_;
  bool verify_certificate_;
  bool download_album_covers_;
  bool server_side_scrobbling_;

  QList<QObject*> wait_for_exit_;

  QStringList errors_;
  QList<QNetworkReply*> replies_;

  bool auto_login_requested_;
  bool login_in_flight_;
  QUrl login_server_url_;  // Server URL and credentials of the login in flight.
  QString login_username_;
  QString login_password_;
  bool login_test_pending_;  // A settings test is waiting for the login in flight.
  bool login_queued_;  // The configured server or credentials changed while a login was in flight.
  bool login_queued_test_;
  QNetworkReply *test_login_reply_;  // A settings test of another server or other credentials.
  QQueue<PendingScrobbleRequest> pending_scrobble_requests_;

  bool reauthenticating_;
  bool pending_catalog_refresh_;
};

using JellyfinServicePtr = SharedPtr<JellyfinService>;

#endif  // JELLYFINSERVICE_H