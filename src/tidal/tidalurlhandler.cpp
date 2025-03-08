/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"
#include "core/song.h"
#include "tidal/tidalservice.h"
#include "tidalurlhandler.h"

TidalUrlHandler::TidalUrlHandler(const SharedPtr<TaskManager> task_manager, TidalService *service)
    : UrlHandler(service),
      task_manager_(task_manager),
      service_(service) {

  QObject::connect(service, &TidalService::StreamURLFailure, this, &TidalUrlHandler::GetStreamURLFailure);
  QObject::connect(service, &TidalService::StreamURLSuccess, this, &TidalUrlHandler::GetStreamURLSuccess);

}

QString TidalUrlHandler::scheme() const {

  return service_->url_scheme();

}

UrlHandler::LoadResult TidalUrlHandler::StartLoading(const QUrl &url) {

  Request request;
  request.task_id = task_manager_->StartTask(QStringLiteral("Loading %1 stream...").arg(url.scheme()));
  QString error;
  request.id = service_->GetStreamURL(url, error);
  if (request.id == 0) {
    CancelTask(request.task_id);
    return LoadResult(url, LoadResult::Type::Error, error);
  }

  requests_.insert(request.id, request);

  LoadResult ret(url);
  ret.type_ = LoadResult::Type::WillLoadAsynchronously;

  return ret;

}

void TidalUrlHandler::GetStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!requests_.contains(id)) return;
  Request req = requests_.take(id);
  CancelTask(req.task_id);

  Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::Error, error));

}

void TidalUrlHandler::GetStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!requests_.contains(id)) return;
  Request req = requests_.take(id);
  CancelTask(req.task_id);

  Q_EMIT AsyncLoadComplete(LoadResult(media_url, LoadResult::Type::TrackAvailable, stream_url, filetype, samplerate, bit_depth, duration));

}

void TidalUrlHandler::CancelTask(const int task_id) {
  task_manager_->SetTaskFinished(task_id);
}
