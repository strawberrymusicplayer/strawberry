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

#ifndef SUBSONICSERVICE_H
#define SUBSONICSERVICE_H

#include "config.h"

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
#include <QNetworkAccessManager>
#include <QSslError>
#include <QDateTime>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "constants/subsonicsettings.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "collection/collectionmodel.h"

class QNetworkReply;

class TaskManager;
class Database;
class UrlHandlers;
class AlbumCoverLoader;
class SubsonicUrlHandler;
class SubsonicRequest;
class SubsonicScrobbleRequest;
class CollectionBackend;
class CollectionModel;
class CollectionFilter;

class SubsonicService : public StreamingService {
  Q_OBJECT

 public:
  explicit SubsonicService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent = nullptr);

  ~SubsonicService() override;

  static const Song::Source kSource;
  static const char *kClientName;
  static const char *kApiVersion;

  void ReloadSettings() override;
  void Exit() override;

  QUrl server_url() const { return server_url_; }
  QString username() const { return username_; }
  QString password() const { return password_; }
  bool http2() const { return http2_; }
  bool verify_certificate() const { return verify_certificate_; }
  bool download_album_covers() const { return download_album_covers_; }
  bool use_album_id_for_album_covers() const { return use_album_id_for_album_covers_; }
  SubsonicSettings::AuthMethod auth_method() const { return auth_method_; }

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  CollectionModel *collection_model() const { return collection_model_; }
  CollectionFilter *collection_filter_model() const { return collection_model_->filter(); }

  SharedPtr<CollectionBackend> songs_collection_backend() override { return collection_backend_; }
  CollectionModel *songs_collection_model() override { return collection_model_; }
  CollectionFilter *songs_collection_filter_model() override { return collection_model_->filter(); }

  void CheckConfiguration();
  void Scrobble(const QString &song_id, const bool submission, const QDateTime &time);

 public Q_SLOTS:
  void SendPing();
  void SendPingWithCredentials(QUrl url, const QString &username, const QString &password, const SubsonicSettings::AuthMethod auth_method, const bool redirect = false);
  void GetSongs() override;
  void DeleteSongs();
  void ResetSongsRequest() override;

 private Q_SLOTS:
  void HandlePingSSLErrors(const QList<QSslError> &ssl_errors);
  void HandlePingReply(QNetworkReply *reply, const QUrl &url, const QString &username, const QString &password, const SubsonicSettings::AuthMethod auth_method);
  void SongsResultsReceived(const SongMap &songs, const QString &error);

 private:
  void PingError(const QString &error = QString(), const QVariant &debug = QVariant());

  ScopedPtr<QNetworkAccessManager> network_;
  SubsonicUrlHandler *url_handler_;

  SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;

  SharedPtr<SubsonicRequest> songs_request_;
  SharedPtr<SubsonicScrobbleRequest> scrobble_request_;

  QUrl server_url_;
  QString username_;
  QString password_;
  bool http2_;
  bool verify_certificate_;
  bool download_album_covers_;
  bool use_album_id_for_album_covers_;
  SubsonicSettings::AuthMethod auth_method_;

  QStringList errors_;
  int ping_redirects_;

  QList<QNetworkReply*> replies_;
};

using SubsonicServicePtr = SharedPtr<SubsonicService>;

#endif  // SUBSONICSERVICE_H
