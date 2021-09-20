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

#include "config.h"

#include <QtGlobal>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "core/song.h"

#include "subsonicservice.h"
#include "subsonicbaserequest.h"
#include "subsonicurlhandler.h"

class Application;

SubsonicUrlHandler::SubsonicUrlHandler(Application *app, SubsonicService *service) : UrlHandler(service), service_(service) {
  Q_UNUSED(app);
}

UrlHandler::LoadResult SubsonicUrlHandler::StartLoading(const QUrl &url) {

  if (!server_url().isValid()) {
    return LoadResult(url, LoadResult::Error, tr("Subsonic server URL is invalid."));
  }

  if (username().isEmpty() || password().isEmpty()) {
    return LoadResult(url, LoadResult::Error, tr("Missing Subsonic username or password."));
  }

  ParamList params = ParamList() << Param("c", client_name())
                                 << Param("v", api_version())
                                 << Param("f", "json")
                                 << Param("u", username())
                                 << Param("id", url.path());

  SubsonicBaseRequest::AddPasswordToParams(params, auth_method(), password());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl stream_url(server_url());

  if (!stream_url.path().isEmpty() && stream_url.path().right(1) == "/") {
    stream_url.setPath(stream_url.path() + QString("rest/stream.view"));
  }
  else {
    stream_url.setPath(stream_url.path() + QString("/rest/stream.view"));
  }

  stream_url.setQuery(url_query);

  return LoadResult(url, LoadResult::TrackAvailable, stream_url);

}
