/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DEEZERURLHANDLER_H
#define DEEZERURLHANDLER_H

#include <QObject>
#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "core/song.h"
#include "deezer/deezerservice.h"

class Application;
class DeezerService;

class DeezerUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  DeezerUrlHandler(Application *app, DeezerService *service);

  QString scheme() const { return service_->url_scheme(); }
  LoadResult StartLoading(const QUrl &url);

  void CancelTask();

 private slots:
  void GetStreamURLFinished(QUrl original_url, QUrl media_url, Song::FileType filetype);

 private:
  Application *app_;
  DeezerService *service_;
  int task_id_;
  QUrl last_original_url_;

};

#endif
