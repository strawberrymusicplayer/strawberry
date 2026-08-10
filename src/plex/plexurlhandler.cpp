/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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

#include <QUrl>
#include <QUrlQuery>

#include "plexservice.h"
#include "plexurlhandler.h"

using namespace Qt::Literals::StringLiterals;

PlexUrlHandler::PlexUrlHandler(PlexService *service) : UrlHandler(service), service_(service) {}

UrlHandler::LoadResult PlexUrlHandler::StartLoading(const QUrl &url) {

  if (!server_url().isValid()) {
    return LoadResult(url, LoadResult::Type::Error, tr("Plex server URL is invalid."));
  }

  if (token().isEmpty()) {
    return LoadResult(url, LoadResult::Type::Error, tr("Not authenticated with Plex."));
  }

  QUrl stream_url(server_url());
  stream_url.setPath(stream_url.path() + url.path());
  QUrlQuery url_query;
  url_query.addQueryItem(u"X-Plex-Token"_s, token());
  stream_url.setQuery(url_query);

  return LoadResult(url, LoadResult::Type::TrackAvailable, stream_url);

}
