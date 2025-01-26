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

#ifndef DROPBOXURLHANDLER_H
#define DROPBOXURLHANDLER_H

#include "includes/shared_ptr.h"
#include "core/urlhandler.h"

class TaskManager;
class DropboxService;

class DropboxUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit DropboxUrlHandler(const SharedPtr<TaskManager> task_manager, DropboxService *service, QObject *parent = nullptr);

  QString scheme() const override;
  LoadResult StartLoading(const QUrl &url) override;

 private:
  void CancelTask(const int task_id);

 private Q_SLOTS:
  void StreamURLRequestFinished(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error = QString());

 private:
  class Request {
   public:
    explicit Request() : id(0), task_id(-1) {}
    uint id;
    int task_id;
  };
  const SharedPtr<TaskManager> task_manager_;
  DropboxService *service_;
  QMap<uint, Request> requests_;
};

#endif  // DROPBOXURLHANDLER_H
