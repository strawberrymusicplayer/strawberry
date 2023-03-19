/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef QOBUZSERVICE_H
#define QOBUZSERVICE_H

#include "config.h"

#include <memory>

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
#include <QSslError>

#include "core/song.h"
#include "internet/internetservice.h"
#include "internet/internetsearchview.h"

class QTimer;
class QNetworkReply;
class QSortFilterProxyModel;
class Application;
class NetworkAccessManager;
class QobuzUrlHandler;
class QobuzRequest;
class QobuzFavoriteRequest;
class QobuzStreamURLRequest;
class CollectionBackend;
class CollectionModel;

class QobuzService : public InternetService {
  Q_OBJECT

 public:
  explicit QobuzService(Application *app, QObject *parent);
  ~QobuzService();

  static const Song::Source kSource;
  static const char kApiUrl[];

  void Exit() override;
  void ReloadSettings() override;

  void Logout();
  int Search(const QString &text, InternetSearchView::SearchType type) override;
  void CancelSearch() override;

  int max_login_attempts() { return kLoginAttempts; }

  Application *app() const { return app_; }
  QString app_id() const { return app_id_; }
  QString app_secret() const { return app_secret_; }
  QString username() const { return username_; }
  QString password() const { return password_; }
  int format() const { return format_; }
  int search_delay() const { return search_delay_; }
  int artistssearchlimit() const { return artistssearchlimit_; }
  int albumssearchlimit() const { return albumssearchlimit_; }
  int songssearchlimit() const { return songssearchlimit_; }
  bool download_album_covers() const { return download_album_covers_; }

  QString user_auth_token() const { return user_auth_token_; }
  qint64 user_id() const { return user_id_; }
  QString device_id() const { return device_id_; }
  qint64 credential_id() const { return credential_id_; }

  bool authenticated() const override { return (!app_id_.isEmpty() && !app_secret_.isEmpty() && !user_auth_token_.isEmpty()); }
  bool login_sent() const { return login_sent_; }
  bool login_attempts() const { return login_attempts_; }

  uint GetStreamURL(const QUrl &url, QString &error);

  CollectionBackend *artists_collection_backend() override { return artists_collection_backend_; }
  CollectionBackend *albums_collection_backend() override { return albums_collection_backend_; }
  CollectionBackend *songs_collection_backend() override { return songs_collection_backend_; }

  CollectionModel *artists_collection_model() override { return artists_collection_model_; }
  CollectionModel *albums_collection_model() override { return albums_collection_model_; }
  CollectionModel *songs_collection_model() override { return songs_collection_model_; }

  QSortFilterProxyModel *artists_collection_sort_model() override { return artists_collection_sort_model_; }
  QSortFilterProxyModel *albums_collection_sort_model() override { return albums_collection_sort_model_; }
  QSortFilterProxyModel *songs_collection_sort_model() override { return songs_collection_sort_model_; }

 public slots:
  void ShowConfig() override;
  void TryLogin();
  void SendLogin();
  void SendLoginWithCredentials(const QString &app_id, const QString &username, const QString &password);
  void GetArtists() override;
  void GetAlbums() override;
  void GetSongs() override;
  void ResetArtistsRequest() override;
  void ResetAlbumsRequest() override;
  void ResetSongsRequest() override;

 private slots:
  void ExitReceived();
  void HandleLoginSSLErrors(const QList<QSslError> &ssl_errors);
  void HandleAuthReply(QNetworkReply *reply);
  void ResetLoginAttempts();
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
  void HandleStreamURLFailure(const uint id, const QUrl &original_url, const QString &error);
  void HandleStreamURLSuccess(const uint id, const QUrl &original_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  QString DecodeAppSecret(const QString &app_secret_encoded) const;
  void SendSearch();
  void LoginError(const QString &error = QString(), const QVariant &debug = QVariant());

  static const char kAuthUrl[];

  static const int kLoginAttempts;
  static const int kTimeResetLoginAttempts;

  static const char kArtistsSongsTable[];
  static const char kAlbumsSongsTable[];
  static const char kSongsTable[];

  static const char kArtistsSongsFtsTable[];
  static const char kAlbumsSongsFtsTable[];
  static const char kSongsFtsTable[];

  Application *app_;
  NetworkAccessManager *network_;
  QobuzUrlHandler *url_handler_;

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

  std::shared_ptr<QobuzRequest> artists_request_;
  std::shared_ptr<QobuzRequest> albums_request_;
  std::shared_ptr<QobuzRequest> songs_request_;
  std::shared_ptr<QobuzRequest> search_request_;
  QobuzFavoriteRequest *favorite_request_;

  QString app_id_;
  QString app_secret_;
  QString username_;
  QString password_;
  int format_;
  int search_delay_;
  int artistssearchlimit_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool download_album_covers_;

  qint64 user_id_;
  QString user_auth_token_;
  QString device_id_;
  qint64 credential_id_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  InternetSearchView::SearchType pending_search_type_;

  int search_id_;
  QString search_text_;
  bool login_sent_;
  int login_attempts_;

  uint next_stream_url_request_id_;
  QMap<uint, std::shared_ptr<QobuzStreamURLRequest>> stream_url_requests_;

  QStringList login_errors_;

  QList<QObject*> wait_for_exit_;
  QList<QNetworkReply*> replies_;

};

#endif  // QOBUZSERVICE_H
