/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "subsonicservice.h"
#include "subsonicurlhandler.h"

class Application;

SubsonicUrlHandler::SubsonicUrlHandler(Application *app, SubsonicService *service) : UrlHandler(service), service_(service) {}

UrlHandler::LoadResult SubsonicUrlHandler::StartLoading(const QUrl &url) {

  if (!server_url().isValid()) {
    return LoadResult(url, LoadResult::Error, url, Song::FileType_Stream, -1, -1, -1, tr("Subsonic server URL is invalid."));
  }

  if (username().isEmpty() || password().isEmpty()) {
    return LoadResult(url, LoadResult::Error, url, Song::FileType_Stream, -1, -1, -1, tr("Missing Subsonic username or password."));
  }

  ParamList params = ParamList() << Param("c", service_->client_name())
                                 << Param("v", service_->api_version())
                                 << Param("f", "json")
                                 << Param("u", service_->username())
                                 << Param("p", QString("enc:" + service_->password().toUtf8().toHex()))
                                 << Param("id", url.path());

  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl media_url(server_url());

  if (!media_url.path().isEmpty() && media_url.path().right(1) == "/") {
    media_url.setPath(media_url.path() + QString("rest/stream"));
  }
  else
    media_url.setPath(media_url.path() + QString("/rest/stream"));

  media_url.setQuery(url_query);

  return LoadResult(url, LoadResult::TrackAvailable, media_url);

}
