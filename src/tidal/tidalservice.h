/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDateTime>
#include <QSslError>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "streaming/streamingsearchview.h"
#include "constants/tidalsettings.h"

class QNetworkReply;
class QTimer;

class TaskManager;
class Database;
class UrlHandlers;
class NetworkAccessManager;
class AlbumCoverLoader;
class TidalUrlHandler;
class TidalRequest;
class TidalFavoriteRequest;
class TidalStreamURLRequest;
class CollectionBackend;
class CollectionModel;
class CollectionFilter;

class TidalService : public StreamingService {
  Q_OBJECT

 public:
  explicit TidalService(const SharedPtr<TaskManager> task_manager,
                        const SharedPtr<Database> database,
                        const SharedPtr<NetworkAccessManager> network,
                        const SharedPtr<UrlHandlers> url_handlers,
                        const SharedPtr<AlbumCoverLoader> albumcover_loader,
                        QObject *parent = nullptr);

  ~TidalService() override;

  static const Song::Source kSource;
  static const char kApiUrl[];
  static const char kResourcesUrl[];

  void Exit() override;
  void ReloadSettings() override;

  void Logout();
  int Search(const QString &text, const SearchType type) override;
  void CancelSearch() override;

  QString client_id() const { return client_id_; }
  quint64 user_id() const { return user_id_; }
  QString country_code() const { return country_code_; }
  QString quality() const { return quality_; }
  int artistssearchlimit() const { return artistssearchlimit_; }
  int albumssearchlimit() const { return albumssearchlimit_; }
  int songssearchlimit() const { return songssearchlimit_; }
  bool fetchalbums() const { return fetchalbums_; }
  QString coversize() const { return coversize_; }
  bool download_album_covers() const { return download_album_covers_; }
  TidalSettings::StreamUrlMethod stream_url_method() const { return stream_url_method_; }
  bool album_explicit() const { return album_explicit_; }

  QString token_type() const { return token_type_; }
  QString access_token() const { return access_token_; }

  bool authenticated() const override { return !token_type_.isEmpty() && !access_token_.isEmpty(); }

  uint GetStreamURL(const QUrl &url, QString &error);

  SharedPtr<CollectionBackend> artists_collection_backend() override { return artists_collection_backend_; }
  SharedPtr<CollectionBackend> albums_collection_backend() override { return albums_collection_backend_; }
  SharedPtr<CollectionBackend> songs_collection_backend() override { return songs_collection_backend_; }

  CollectionModel *artists_collection_model() override { return artists_collection_model_; }
  CollectionModel *albums_collection_model() override { return albums_collection_model_; }
  CollectionModel *songs_collection_model() override { return songs_collection_model_; }

  CollectionFilter *artists_collection_filter_model() override { return artists_collection_model_->filter(); }
  CollectionFilter *albums_collection_filter_model() override { return albums_collection_model_->filter(); }
  CollectionFilter *songs_collection_filter_model() override { return songs_collection_model_->filter(); }

 public Q_SLOTS:
  void StartAuthorization(const QString &client_id);
  void GetArtists() override;
  void GetAlbums() override;
  void GetSongs() override;
  void ResetArtistsRequest() override;
  void ResetAlbumsRequest() override;
  void ResetSongsRequest() override;
  void AuthorizationUrlReceived(const QUrl &url);

 private Q_SLOTS:
  void ExitReceived();
  void RequestNewAccessToken() { RequestAccessToken(); }
  void HandleLoginSSLErrors(const QList<QSslError> &ssl_errors);
  void AccessTokenRequestFinished(QNetworkReply *reply);
  void StartSearch();
  void ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void SongsResultsReceived(const int id, const SongMap &songs, const QString &error);
  void SearchResultsReceived(const int id, const SongMap &songs, const QString &error);
  void ArtistsUpdateStatusReceived(const int id, const QString &text);
  void AlbumsUpdateStatusReceived(const int id, const QString &text);
  void SongsUpdateStatusReceived(const int id, const QString &text);
  void ArtistsUpdateProgressReceived(const int id, const int progress);
  void AlbumsUpdateProgressReceived(const int id, const int progress);
  void SongsUpdateProgressReceived(const int id, const int progress);
  void HandleStreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void HandleStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  void LoadSession();
  void RequestAccessToken(const QString &code = QString());
  void SendSearch();
  void LoginError(const QString &error = QString(), const QVariant &debug = QVariant());

  const SharedPtr<NetworkAccessManager> network_;
  TidalUrlHandler *url_handler_;

  SharedPtr<CollectionBackend> artists_collection_backend_;
  SharedPtr<CollectionBackend> albums_collection_backend_;
  SharedPtr<CollectionBackend> songs_collection_backend_;

  CollectionModel *artists_collection_model_;
  CollectionModel *albums_collection_model_;
  CollectionModel *songs_collection_model_;

  QTimer *timer_search_delay_;
  QTimer *timer_refresh_login_;

  SharedPtr<TidalRequest> artists_request_;
  SharedPtr<TidalRequest> albums_request_;
  SharedPtr<TidalRequest> songs_request_;
  SharedPtr<TidalRequest> search_request_;
  TidalFavoriteRequest *favorite_request_;

  bool enabled_;
  QString client_id_;
  quint64 user_id_;
  QString country_code_;
  QString quality_;
  int artistssearchlimit_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool fetchalbums_;
  QString coversize_;
  bool download_album_covers_;
  TidalSettings::StreamUrlMethod stream_url_method_;
  bool album_explicit_;

  QString token_type_;
  QString access_token_;
  QString refresh_token_;
  quint64 expires_in_;
  quint64 login_time_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  SearchType pending_search_type_;

  int search_id_;
  QString search_text_;

  QString code_verifier_;
  QString code_challenge_;

  uint next_stream_url_request_id_;
  QMap<uint, SharedPtr<TidalStreamURLRequest>> stream_url_requests_;

  QStringList login_errors_;

  QList<QObject*> wait_for_exit_;
  QList<QNetworkReply*> replies_;
};

using TidalServicePtr = SharedPtr<TidalService>;

#endif  // TIDALSERVICE_H
