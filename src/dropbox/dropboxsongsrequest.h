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

#ifndef DROPBOXSONGSREQUEST_H
#define DROPBOXSONGSREQUEST_H

#include "config.h"

#include <QList>
#include <QString>
#include <QUrl>

#include "dropboxbaserequest.h"

class NetworkAccessManager;
class CollectionBackend;
class QNetworkReply;
class DropboxService;

class DropboxSongsRequest : public DropboxBaseRequest {
  Q_OBJECT

 public:
  explicit DropboxSongsRequest(const SharedPtr<NetworkAccessManager> network, const SharedPtr<CollectionBackend> collection_backend, DropboxService *service, QObject *parent = nullptr);

  void ReloadSettings();

  void GetFolderList();

 Q_SIGNALS:
  void ShowErrorDialog(const QString &error);

 private:
  void LongPollDelta();
  void GetStreamURL(const QUrl &url, const QString &path, const qint64 size, const qint64 mtime);

 protected:
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;

 private Q_SLOTS:
  void GetFolderListFinished(QNetworkReply *reply);
  void LongPollDeltaFinished(QNetworkReply *reply);
  void GetStreamUrlFinished(QNetworkReply *reply, const QString &filename, const qint64 size, const qint64 mtime);

 private:
  const SharedPtr<NetworkAccessManager> network_;
  const SharedPtr<CollectionBackend> collection_backend_;
  DropboxService *service_;
};

#endif  // DROPBOXSONGSREQUEST_H
