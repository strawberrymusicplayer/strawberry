/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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

#ifndef PLEXSERVICE_H
#define PLEXSERVICE_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QSslError>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "streaming/streamingservice.h"
#include "collection/collectionmodel.h"

class QNetworkReply;

class TaskManager;
class Database;
class UrlHandlers;
class AlbumCoverLoader;
class PlexUrlHandler;
class PlexRequest;
class CollectionBackend;
class CollectionModel;
class CollectionFilter;

class PlexService : public StreamingService {
  Q_OBJECT

 public:
  explicit PlexService(const SharedPtr<TaskManager> task_manager,
                       const SharedPtr<Database> database,
                       const SharedPtr<UrlHandlers> url_handlers,
                       const SharedPtr<AlbumCoverLoader> albumcover_loader,
                       QObject *parent = nullptr);

  ~PlexService() override;

  static const Song::Source kSource;

  struct Connection {
    QUrl uri;
    QString protocol;
    bool local = false;
    bool relay = false;
  };
  using ConnectionList = QList<Connection>;

  struct Server {
    QString name;
    QUrl url;  // Best-guess/display URL only; the actual connection used is chosen from "connections" by testing reachability.
    QString access_token;
    bool owned = false;
    QString machine_identifier;
    ConnectionList connections;
  };
  using ServerList = QList<Server>;

  void ReloadSettings() override;
  void Exit() override;

  bool oauth() const override { return true; }
  bool authenticated() const override { return !token_.isEmpty(); }

  QUrl server_url() const { return server_url_; }
  QString token() const { return token_; }
  // Token to use against the configured server: shared (non-owned) servers require their own access token.
  QString server_token() const { return server_token_.isEmpty() ? token_ : server_token_; }
  QString client_id() const { return client_id_; }
  bool verify_certificate() const { return verify_certificate_; }
  bool download_album_covers() const { return download_album_covers_; }

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  CollectionModel *collection_model() const { return collection_model_; }

  SharedPtr<CollectionBackend> songs_collection_backend() override { return collection_backend_; }
  CollectionModel *songs_collection_model() override { return collection_model_; }
  CollectionFilter *songs_collection_filter_model() override { return collection_model_->filter(); }

  // Shared QNetworkAccessManager, reused by PlexBaseRequest instead of creating a new one per request.
  SharedPtr<QNetworkAccessManager> network();

 public Q_SLOTS:
  void Authenticate();
  void CancelAuthentication();
  void Deauthenticate();
  void GetServers();
  void SendPing();
  void SendPingWithSettings(const QUrl &url, const QString &token);
  void GetSongs() override;
  void DeleteSongs();
  void ResetSongsRequest() override;

 Q_SIGNALS:
  void ServersFound(const PlexService::ServerList &servers);

 private Q_SLOTS:
  void HandlePinReply(QNetworkReply *reply);
  void PollPin();
  void HandlePollPinReply(QNetworkReply *reply);
  void HandleResourcesReply(QNetworkReply *reply);
  void HandlePingReply(QNetworkReply *reply);
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);
  void SongsResultsReceived(const SongMap &songs, const QString &error);

 private:
  QNetworkRequest CreatePlexTvRequest(const QUrl &url) const;
  void FinishAuthentication(const QString &token);
  void AuthenticationError(const QString &error);
  void PingError(const QString &error);

  // Reachability-based selection among all of a server's advertised connections (see plexservice.cpp for rationale).
  static ConnectionList OrderConnectionCandidates(const ConnectionList &connections);
  static bool HostLikelyReachableOnLan(const QString &host);
  void BeginConnectionSelection(const Server &server);
  void TryNextConnectionCandidate();
  void HandleConnectionTestReply(QNetworkReply *reply, const QUrl &candidate_uri);

  SharedPtr<QNetworkAccessManager> network_;
  PlexUrlHandler *url_handler_;

  SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;

  SharedPtr<PlexRequest> songs_request_;

  QUrl server_url_;
  QString token_;
  QString server_token_;
  QString client_id_;
  bool verify_certificate_;
  bool download_album_covers_;
  qint64 last_update_;

  QTimer timer_pin_poll_;
  qint64 pin_id_;
  QString pin_code_;
  int pin_polls_remaining_;
  QNetworkReply *pin_poll_reply_;

  QStringList errors_;
  QList<QNetworkReply*> replies_;

  ConnectionList connection_test_candidates_;
  int connection_test_index_;
  Server connection_test_server_;
  QNetworkReply *connection_test_reply_;
};

using PlexServicePtr = SharedPtr<PlexService>;

Q_DECLARE_METATYPE(PlexService::ServerList)

#endif  // PLEXSERVICE_H
