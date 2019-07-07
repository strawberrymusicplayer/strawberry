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

#ifndef TIDALSERVICE_H
#define TIDALSERVICE_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkReply>
#include <QTimer>
#include <QSortFilterProxyModel>

#include "core/song.h"
#include "internet/internetservice.h"
#include "internet/internetsearch.h"
#include "settings/tidalsettingspage.h"

class Application;
class NetworkAccessManager;
class TidalUrlHandler;
class TidalRequest;
class TidalFavoriteRequest;
class TidalStreamURLRequest;
class CollectionBackend;
class CollectionModel;

using std::shared_ptr;

class TidalService : public InternetService {
  Q_OBJECT

 public:
  TidalService(Application *app, QObject *parent);
  ~TidalService();

  static const Song::Source kSource;

  void ReloadSettings();

  void Logout();
  int Search(const QString &query, InternetSearch::SearchType type);
  void CancelSearch();

  const int max_login_attempts() { return kLoginAttempts; }

  Application *app() { return app_; }

  const bool oauth() { return oauth_; }
  QString client_id() { return client_id_; }
  QString api_token() { return api_token_; }
  quint64 user_id() { return user_id_; }
  QString country_code() { return country_code_; }
  QString username() { return username_; }
  QString password() { return password_; }
  QString quality() { return quality_; }
  int search_delay() { return search_delay_; }
  int artistssearchlimit() { return artistssearchlimit_; }
  int albumssearchlimit() { return albumssearchlimit_; }
  int songssearchlimit() { return songssearchlimit_; }
  bool fetchalbums() { return fetchalbums_; }
  QString coversize() { return coversize_; }
  bool download_album_covers() { return download_album_covers_; }
  TidalSettingsPage::StreamUrlMethod stream_url_method() { return stream_url_method_; }

  QString access_token() { return access_token_; }
  QString session_id() { return session_id_; }

  const bool authenticated() { return (!access_token_.isEmpty() || !session_id_.isEmpty()); }
  const bool login_sent() { return login_sent_; }
  const bool login_attempts() { return login_attempts_; }

  void GetStreamURL(const QUrl &url);

  CollectionBackend *artists_collection_backend() { return artists_collection_backend_; }
  CollectionBackend *albums_collection_backend() { return albums_collection_backend_; }
  CollectionBackend *songs_collection_backend() { return songs_collection_backend_; }

  CollectionModel *artists_collection_model() { return artists_collection_model_; }
  CollectionModel *albums_collection_model() { return albums_collection_model_; }
  CollectionModel *songs_collection_model() { return songs_collection_model_; }

  QSortFilterProxyModel *artists_collection_sort_model() { return artists_collection_sort_model_; }
  QSortFilterProxyModel *albums_collection_sort_model() { return albums_collection_sort_model_; }
  QSortFilterProxyModel *songs_collection_sort_model() { return songs_collection_sort_model_; }

  enum QueryType {
    QueryType_Artists,
    QueryType_Albums,
    QueryType_Songs,
    QueryType_SearchArtists,
    QueryType_SearchAlbums,
    QueryType_SearchSongs,
  };

 signals:

 public slots:
  void ShowConfig();
  void TryLogin();
  void SendLogin(const QString &api_token, const QString &username, const QString &password);
  void GetArtists();
  void GetAlbums();
  void GetSongs();
  void ResetArtistsRequest();
  void ResetAlbumsRequest();
  void ResetSongsRequest();

 private slots:
  void StartAuthorisation();
  void AuthorisationUrlReceived(const QUrl &url);
  void HandleLoginSSLErrors(QList<QSslError> ssl_errors);
  void AccessTokenRequestFinished(QNetworkReply *reply);
  void SendLogin();
  void HandleAuthReply(QNetworkReply *reply);
  void ResetLoginAttempts();
  void StartSearch();
  void ArtistsResultsReceived(const int id, const SongList &songs, const QString &error);
  void AlbumsResultsReceived(const int id, const SongList &songs, const QString &error);
  void SongsResultsReceived(const int id, const SongList &songs, const QString &error);
  void SearchResultsReceived(const int id, const SongList &songs, const QString &error);
  void ArtistsUpdateStatusReceived(const int id, const QString &text);
  void AlbumsUpdateStatusReceived(const int id, const QString &text);
  void SongsUpdateStatusReceived(const int id, const QString &text);
  void ArtistsProgressSetMaximumReceived(const int id, const int max);
  void AlbumsProgressSetMaximumReceived(const int id, const int max);
  void SongsProgressSetMaximumReceived(const int id, const int max);
  void ArtistsUpdateProgressReceived(const int id, const int progress);
  void AlbumsUpdateProgressReceived(const int id, const int progress);
  void SongsUpdateProgressReceived(const int id, const int progress);
  void HandleStreamURLFinished(const QUrl &original_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration, QString error = QString());

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<EncodedParam> EncodedParamList;

  void SendSearch();
  void LoginError(const QString &error = QString(), const QVariant &debug = QVariant());

  static const char *kApiTokenB64;
  static const char *kOAuthUrl;
  static const char *kOAuthAccessTokenUrl;
  static const char *kOAuthRedirectUrl;
  static const char *kAuthUrl;
  static const int kLoginAttempts;
  static const int kTimeResetLoginAttempts;

  static const char *kArtistsSongsTable;
  static const char *kAlbumsSongsTable;
  static const char *kSongsTable;

  static const char *kArtistsSongsFtsTable;
  static const char *kAlbumsSongsFtsTable;
  static const char *kSongsFtsTable;

  Application *app_;
  NetworkAccessManager *network_;
  TidalUrlHandler *url_handler_;

  CollectionBackend *artists_collection_backend_;
  CollectionBackend *albums_collection_backend_;
  CollectionBackend *songs_collection_backend_;

  CollectionModel *artists_collection_model_;
  CollectionModel *albums_collection_model_;
  CollectionModel *songs_collection_model_;

  QSortFilterProxyModel *artists_collection_sort_model_;
  QSortFilterProxyModel *albums_collection_sort_model_;
  QSortFilterProxyModel *songs_collection_sort_model_;

  QTimer *timer_search_delay_;
  QTimer *timer_login_attempt_;

  std::shared_ptr<TidalRequest> artists_request_;
  std::shared_ptr<TidalRequest> albums_request_;
  std::shared_ptr<TidalRequest> songs_request_;
  std::shared_ptr<TidalRequest> search_request_;
  TidalFavoriteRequest *favorite_request_;

  bool oauth_;
  QString client_id_;
  QString api_token_;
  quint64 user_id_;
  QString country_code_;
  QString username_;
  QString password_;
  QString quality_;
  int search_delay_;
  int artistssearchlimit_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool fetchalbums_;
  QString coversize_;
  bool download_album_covers_;
  TidalSettingsPage::StreamUrlMethod stream_url_method_;

  QString access_token_;
  QString refresh_token_;
  QString session_id_;
  QDateTime expiry_time_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  InternetSearch::SearchType pending_search_type_;

  int search_id_;
  QString search_text_;
  bool login_sent_;
  int login_attempts_;

  QString code_verifier_;
  QString code_challenge_;

  QList<TidalStreamURLRequest*> stream_url_requests_;

  QStringList login_errors_;

};

#endif  // TIDALSERVICE_H
