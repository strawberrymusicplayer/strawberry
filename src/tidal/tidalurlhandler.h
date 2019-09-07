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

#ifndef TIDALURLHANDLER_H
#define TIDALURLHANDLER_H

#include <QObject>
#include <QString>
#include <QUrl>

#include "core/urlhandler.h"
#include "core/song.h"
#include "tidal/tidalservice.h"

class Application;
class TidalService;

class TidalUrlHandler : public UrlHandler {
  Q_OBJECT

 public:
  TidalUrlHandler(Application *app, TidalService *service);

  QString scheme() const { return service_->url_scheme(); }
  LoadResult StartLoading(const QUrl &url);

  void CancelTask();

 private slots:
  void GetStreamURLFinished(const QUrl &original_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration, QString error = QString());

 private:
  Application *app_;
  TidalService *service_;
  int task_id_;

};

#endif
