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

#ifndef QOBUZURLHANDLER_H
#define QOBUZURLHANDLER_H

#include <QMap>
#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "core/song.h"
#include "qobuz/qobuzservice.h"

class TaskManager;

class QobuzUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  explicit QobuzUrlHandler(const SharedPtr<TaskManager> task_manager, QobuzService *service);

  QString scheme() const { return service_->url_scheme(); }
  LoadResult StartLoading(const QUrl &url);

 private:
  void CancelTask(const int task_id);

 private Q_SLOTS:
  void GetStreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void GetStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);

 private:
  struct Request {
    Request() : id(0), task_id(-1) {}
    uint id;
    int task_id;
  };
  const SharedPtr<TaskManager> task_manager_;
  QobuzService *service_;
  QMap<uint, Request> requests_;
};

#endif  // QOBUZURLHANDLER_H
