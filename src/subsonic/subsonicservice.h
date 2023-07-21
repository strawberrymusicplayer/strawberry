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

#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "core/song.h"
#include "internet/internetservice.h"
#include "settings/subsonicsettingspage.h"

class QSortFilterProxyModel;
class QNetworkReply;

class Application;
class SubsonicUrlHandler;
class SubsonicRequest;
class SubsonicScrobbleRequest;
class CollectionBackend;
class CollectionModel;

class SubsonicService : public InternetService {
  Q_OBJECT

 public:
  explicit SubsonicService(Application *app, QObject *parent = nullptr);
  ~SubsonicService() override;

  static const Song::Source kSource;
  static const char *kClientName;
  static const char *kApiVersion;

  void ReloadSettings() override;
  void Exit() override;

  Application *app() const { return app_; }

  QUrl server_url() const { return server_url_; }
  QString username() const { return username_; }
  QString password() const { return password_; }
  bool http2() const { return http2_; }
  bool verify_certificate() const { return verify_certificate_; }
  bool download_album_covers() const { return download_album_covers_; }
  SubsonicSettingsPage::AuthMethod auth_method() const { return auth_method_; }

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  CollectionModel *collection_model() const { return collection_model_; }
  QSortFilterProxyModel *collection_sort_model() const { return collection_sort_model_; }

  SharedPtr<CollectionBackend> songs_collection_backend() override { return collection_backend_; }
  CollectionModel *songs_collection_model() override { return collection_model_; }
  QSortFilterProxyModel *songs_collection_sort_model() override { return collection_sort_model_; }

  void CheckConfiguration();
  void Scrobble(const QString &song_id, const bool submission, const QDateTime &time);

 public slots:
  void ShowConfig() override;
  void SendPing();
  void SendPingWithCredentials(QUrl url, const QString &username, const QString &password, const SubsonicSettingsPage::AuthMethod auth_method, const bool redirect = false);
  void GetSongs() override;
  void DeleteSongs();
  void ResetSongsRequest() override;

 private slots:
  void HandlePingSSLErrors(const QList<QSslError> &ssl_errors);
  void HandlePingReply(QNetworkReply *reply, const QUrl &url, const QString &username, const QString &password, const SubsonicSettingsPage::AuthMethod auth_method);
  void SongsResultsReceived(const SongMap &songs, const QString &error);

 private:
  void PingError(const QString &error = QString(), const QVariant &debug = QVariant());

  static const char *kSongsTable;
  static const char *kSongsFtsTable;
  static const int kMaxRedirects;

  Application *app_;
  ScopedPtr<QNetworkAccessManager> network_;
  SubsonicUrlHandler *url_handler_;

  SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;
  QSortFilterProxyModel *collection_sort_model_;

  SharedPtr<SubsonicRequest> songs_request_;
  SharedPtr<SubsonicScrobbleRequest> scrobble_request_;

  QUrl server_url_;
  QString username_;
  QString password_;
  bool http2_;
  bool verify_certificate_;
  bool download_album_covers_;
  SubsonicSettingsPage::AuthMethod auth_method_;

  QStringList errors_;
  int ping_redirects_;

  QList<QNetworkReply*> replies_;
};

using SubsonicServicePtr = SharedPtr<SubsonicService>;

#endif  // SUBSONICSERVICE_H
