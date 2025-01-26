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

#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"
#include "dropboxurlhandler.h"
#include "dropboxservice.h"

DropboxUrlHandler::DropboxUrlHandler(const SharedPtr<TaskManager> task_manager, DropboxService *service, QObject *parent)
    : UrlHandler(parent),
      task_manager_(task_manager),
      service_(service) {

  QObject::connect(service, &DropboxService::StreamURLRequestFinished, this, &DropboxUrlHandler::StreamURLRequestFinished);

}

QString DropboxUrlHandler::scheme() const { return service_->url_scheme(); }

UrlHandler::LoadResult DropboxUrlHandler::StartLoading(const QUrl &url) {

  Request request;
  request.task_id = task_manager_->StartTask(QStringLiteral("Loading %1 stream...").arg(url.scheme()));
  QString error;
  request.id = service_->GetStreamURL(url, error);
  if (request.id == 0) {
    CancelTask(request.task_id);
    return LoadResult(url, LoadResult::Type::Error, error);
  }

  requests_.insert(request.id, request);

  LoadResult load_result(url);
  load_result.type_ = LoadResult::Type::WillLoadAsynchronously;

  return load_result;

}

void DropboxUrlHandler::StreamURLRequestFinished(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error) {

  if (!requests_.contains(id)) return;
  const Request request = requests_.take(id);
  CancelTask(request.task_id);

  if (success) {
    Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::TrackAvailable, stream_url));
  }
  else {
    Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::Error, error));
  }

}

void DropboxUrlHandler::CancelTask(const int task_id) {
  task_manager_->SetTaskFinished(task_id);
}
