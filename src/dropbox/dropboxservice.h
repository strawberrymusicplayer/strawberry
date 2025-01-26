/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DROPBOXSERVICE_H
#define DROPBOXSERVICE_H

#include <QList>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "core/song.h"
#include "streaming/cloudstoragestreamingservice.h"

class QNetworkReply;

class TaskManager;
class Database;
class NetworkAccessManager;
class UrlHandlers;
class TagReaderClient;
class AlbumCoverLoader;
class OAuthenticator;
class DropboxSongsRequest;
class DropboxStreamURLRequest;

class DropboxService : public CloudStorageStreamingService {
  Q_OBJECT

 public:
  explicit DropboxService(const SharedPtr<TaskManager> task_manager,
                          const SharedPtr<Database> database,
                          const SharedPtr<NetworkAccessManager> network,
                          const SharedPtr<UrlHandlers> url_handlers,
                          const SharedPtr<TagReaderClient> tagreader_client,
                          const SharedPtr<AlbumCoverLoader> albumcover_loader,
                          QObject *parent = nullptr);

  static const Song::Source kSource;

  bool oauth() const override { return true; }
  bool authenticated() const override;
  bool show_progress() const override { return false; }
  bool enable_refresh_button() const override { return false; }

  void Exit() override;
  void ReloadSettings() override;

  void Authenticate();
  void ClearSession();

  void Start();
  void Reset();
  uint GetStreamURL(const QUrl &url, QString &error);

  QByteArray authorization_header() const;

 Q_SIGNALS:
  void StreamURLRequestFinished(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error = QString());

 private Q_SLOTS:
  void ExitReceived();
  void OAuthFinished(const bool success, const QString &error = QString());
  void StreamURLRequestFinishedSlot(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error = QString());

 private:
  const SharedPtr<NetworkAccessManager> network_;
  OAuthenticator *oauth_;
  DropboxSongsRequest *songs_request_;
  bool enabled_;
  QList<QObject*> wait_for_exit_;
  bool finished_;
  uint next_stream_url_request_id_;
  QMap<uint, QSharedPointer<DropboxStreamURLRequest>> stream_url_requests_;
};

#endif  // DROPBOXSERVICE_H
