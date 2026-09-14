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
#include <QList>
#include <QString>
#include <QUrl>
#include <QElapsedTimer>
#include <QQueue>
#include <QScopedPointer>
#include <QSslError>
#include <QNetworkRequest>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "credentialsmanager/credentialsreply.h"
#include "collection/collectionmodel.h"

class QTimer;
class QNetworkReply;

class TaskManager;
class Database;
class NetworkAccessManager;
class CredentialsManager;
class UrlHandlers;
class AlbumCoverLoader;
class CollectionBackend;
class CollectionFilter;
class JellyfinRequest;
class JellyfinScrobbleRequest;
class JellyfinFavoriteRequest;
class JellyfinUrlHandler;

// A favorite artist, album or song to add or remove.
struct JellyfinFavoriteChange {
  int catalog;
  QString item_id;
  bool add;
  bool retry;  // Sent again after logging in, because the access token was rejected.
};
using JellyfinFavoriteChangeList = QList<JellyfinFavoriteChange>;

using JellyfinRequestPtr = QScopedPointer<JellyfinRequest, QScopedPointerDeleteLater>;

class JellyfinService : public StreamingService {
  Q_OBJECT

 public:
  explicit JellyfinService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<NetworkAccessManager> network,
                           const SharedPtr<CredentialsManager> credentials_manager,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent = nullptr);

  ~JellyfinService() override;

  static const Song::Source kSource;

  // The favorite artists, albums and songs catalogs shown by the streaming view.
  enum CatalogType {
    CatalogNone = 0,  // For example a search.
    CatalogArtists = 1,
    CatalogAlbums = 2,
    CatalogSongs = 4,
  };

  void ReloadSettings() override;
  void Exit() override;

  bool authenticated() const override { return !access_token_.isEmpty(); }

  SharedPtr<CredentialsManager> credentials_manager() const { return credentials_manager_; }

  // Starts a login if needed and allowed, returns true if a login is in flight and AuthFinished will be emitted.
  bool EnsureLogin();

  QUrl server_url() const { return server_url_; }
  QString access_token() const { return access_token_; }
  QString user_id() const { return user_id_; }
  bool http2() const { return http2_; }
  bool verify_certificate() const { return verify_certificate_; }
  bool download_album_covers() const { return download_album_covers_; }

  QUrl GetStreamUrl(const QString &song_id) const;
  void ReportPlaybackStart(const QString &song_id);
  void ReportPlaybackProgress(const QString &song_id, const qint64 position_nanosec, const bool paused);
  void ReportPlaybackStopped(const QString &song_id, const qint64 position_nanosec);

  QString CreateAuthorizationHeader(const bool with_token = false) const;

  // Requests use QNetworkRequest::UserVerifiedRedirectPolicy, this only follows redirects to the same server.
  static void HandleRedirects(QNetworkReply *reply);
  static QString RedirectRefusedError(QNetworkReply *reply);

  // The URL of a resource path on the server, the server URL can have a path.
  static QUrl ServerUrl(const QUrl &server_url, const QString &ressource_path);

  // The server accepted the access token, it's checked again before playing when it was not used for a while.
  void TokenValidated();
  bool TokenNeedsValidation() const;
  // Check the access token before the next song plays, for example after playing a song failed.
  void TokenNeedsCheck() { token_validated_timer_.invalidate(); }
  // Checks that the server still accepts the access token and logs in again if not, TokenValidationFinished or AuthFinished is emitted.
  void ValidateToken();

  void Reauthenticate(const QString &rejected_access_token = QString());
  bool Unauthorized(const QString &rejected_access_token, const int catalog);

  // The settings page saved a new password, read it again on the next settings reload.
  void PasswordSaved() { password_saved_ = true; }

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
  void SendPingWithCredentials(const QUrl &url, const QString &username, const QString &password, const bool http2, const bool verify_certificate);

  void GetArtists() override;
  void GetAlbums() override;
  void GetSongs() override;

  void ResetArtistsRequest() override;
  void ResetAlbumsRequest() override;
  void ResetSongsRequest() override;

 Q_SIGNALS:
  void ScrobbleError(const QString &error);
  // A login finished, successful or not, named differently from StreamingService::LoginFinished(), which has another meaning.
  void AuthFinished();
  void TokenValidationFinished();
  // Playback reports were dropped, for example when the server changed, the scrobbler stops reporting the playing song.
  void PlaybackReportsCleared();

 private Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void HandleAuthReply(QNetworkReply *reply, const QUrl &server_url, const QString &username, const QString &password);
  void HandleTestAuthReply(QNetworkReply *reply, const QUrl &server_url, const bool http2, const bool verify_certificate);
  void TokenValidationReplyReceived(QNetworkReply *reply, const QString &access_token);
  void ReadPasswordFinished();
  void ExitReceived();

  void CatalogResultsReceived(const int catalog, const SongMap &songs, const QString &error);
  void CatalogUpdateStatusReceived(const int catalog, const QString &text);
  void CatalogUpdateProgressReceived(const int catalog, const int progress);

  void SearchResultsReceived(const int id, const SongMap &songs, const QString &error);
  void SearchUpdateStatusReceived(const int id, const QString &text);
  void SearchUpdateProgressReceived(const int id, const int progress);

  void StartSearch();
  void SearchLoginFinished();

  void AddArtistsToFavorites(const SongList &songs);
  void AddAlbumsToFavorites(const SongList &songs);
  void AddSongsToFavorites(const SongList &songs);
  void RemoveArtistsFromFavorites(const SongList &songs);
  void RemoveAlbumsFromFavorites(const SongList &songs);
  void RemoveSongsFromFavorites(const SongList &songs);
  void RemoveSongMapFromFavorites(const SongMap &songs);
  void FavoritesChanged(const int catalog, const QString &error);
  void FavoritesUnauthorized(const QString &access_token, const JellyfinFavoriteChangeList &changes);
  void SendPendingFavoriteChanges();

 private:
  // True while the password is being read from the credentials manager, logins wait until it's loaded.
  bool password_loading() const { return static_cast<bool>(read_password_reply_); }

  struct AuthReply {
    AuthReply() : credentials_rejected(false), network_error(false) {}
    QString error;
    bool credentials_rejected;
    bool network_error;  // The server could not be reached or had an error, this doesn't count towards the server's lockout of the account.
    QString access_token;
    QString user_id;
  };

  static QUrl NormalizedServerUrl(const QUrl &url);
  static QUrl AuthUrl(const QUrl &server_url);
  bool IsConfiguredLogin(const QUrl &server_url, const QString &username, const QString &password) const;
  void Login(const bool test);
  QNetworkReply *CreateAuthenticateRequest(const QUrl &url, const QString &username, const QString &password, const bool test_device, const bool http2, const bool verify_certificate);
  QString CreateAuthorizationHeader(const QString &access_token, const bool test_device) const;
  QNetworkRequest CreateServerRequest(const QUrl &url, const QString &access_token, const bool test_device, const bool http2, const bool verify_certificate) const;
  void LogoutTestSession(const QUrl &server_url, const QString &access_token, const bool http2, const bool verify_certificate);
  void TokenRejected(const QString &access_token);
  SharedPtr<JellyfinScrobbleRequest> ScrobbleRequest();
  AuthReply ParseAuthReply(QNetworkReply *reply) const;
  void SetAuth(const QString &access_token, const QString &user_id, const QUrl &server_url, const QString &username);
  void AuthError(const AuthReply &auth_reply, const bool test);
  bool AutomaticLoginAllowed() const;
  bool CanLogin() const;
  void ResetRequest(JellyfinRequestPtr &request);
  void ChangeFavorites(const int catalog, const SongList &songs, const bool add);
  void AbortRequests(const QString &error);
  bool HasCredentials() const;
  void SetPassword(const QString &password);
  void PasswordLoadFinished();
  void CatalogLoginFailed(const QString &error);
  bool DeferCatalogRequest(const int catalogs);
  void GetCatalogs(const int catalogs);
  void StartCatalogRequest(const int catalog);
  JellyfinRequestPtr &CatalogRequest(const int catalog);
  void EmitCatalogResults(const int catalogs, const SongMap &songs, const QString &error);
  void EmitTestResult(const QString &error);
  void FlushPendingScrobbles();

  // A playback report that arrived while the service was not yet authenticated.
  // It is buffered and sent once a login completes.
  struct PendingScrobbleRequest {
    enum class Type : int {
      Start,
      Progress,
      Stopped,
    } type;
    QString song_id;
    qint64 position_nanosec;
    bool paused;
  };
  void ReportPlayback(const PendingScrobbleRequest &request);

  const SharedPtr<NetworkAccessManager> network_;
  const SharedPtr<CredentialsManager> credentials_manager_;

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
  JellyfinFavoriteRequest *favorite_request_;
  JellyfinFavoriteChangeList pending_favorite_changes_;  // Changes waiting for a login.

  QTimer *timer_search_delay_;
  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  SearchType pending_search_type_;
  int search_id_;

  QUrl server_url_;
  QString username_;
  QString password_;
  mutable QString device_id_;
  QString failed_password_;  // The password of the last failed login, setting it again doesn't allow automatic logins again.
  QString access_token_;
  QString user_id_;
  QUrl authenticated_server_url_;  // Normalized server URL and username the access token belongs to.
  QString authenticated_username_;
  bool http2_;
  bool verify_certificate_;
  bool download_album_covers_;
  bool enabled_;

  QList<QObject*> wait_for_exit_;

  QList<QNetworkReply*> replies_;

  bool login_in_flight_;
  QUrl login_server_url_;  // Server URL and credentials of the login in flight.
  QString login_username_;
  QString login_password_;
  bool login_test_pending_;  // A settings test is waiting for the login in flight.
  bool login_queued_;  // The configured server or credentials changed while a login was in flight.
  bool login_queued_test_;
  QNetworkReply *test_login_reply_;  // A settings test of another server or other credentials.
  bool login_rejected_;  // The configured credentials were rejected, don't log in automatically until they change.
  QElapsedTimer token_validated_timer_;  // Started when the server accepted the access token.
  QNetworkReply *token_validation_reply_;
  QElapsedTimer login_failed_timer_;  // Started on a failed login, automatic logins wait a while after a failure.
  int catalog_401_logins_;  // Logins after catalog or search 401s without a successful load in between.
  bool password_saved_;

  CredentialsReplyPtr read_password_reply_;
  bool login_after_password_load_;  // A login was requested while the password was being read.
  bool login_after_password_load_test_;
  QQueue<PendingScrobbleRequest> pending_scrobble_requests_;

  bool reauthenticating_;
  int pending_catalogs_;  // The catalogs to load when the login in progress finished.
  bool search_after_login_;  // A search is waiting for a login to finish.
};

using JellyfinServicePtr = SharedPtr<JellyfinService>;

#endif  // JELLYFINSERVICE_H
