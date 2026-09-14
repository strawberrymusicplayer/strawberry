/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include "core/song.h"
#include "jellyfin/jellyfinservice.h"
#include "jellyfinurlhandler.h"

JellyfinUrlHandler::JellyfinUrlHandler(JellyfinService *service)
    : UrlHandler(service),
      service_(service) {}

UrlHandler::LoadResult JellyfinUrlHandler::StartLoading(const QUrl &media_url) {

  if (!service_->authenticated()) {
    return LoadResult(media_url, LoadResult::Type::Error, tr("Not authenticated with Jellyfin."));
  }

  QString song_id = media_url.host();
  if (song_id.isEmpty()) {
    QString path = media_url.path();
    if (path.startsWith(QLatin1Char('/'))) path = path.mid(1);
    song_id = path;
  }
  if (song_id.isEmpty()) {
    return LoadResult(media_url, LoadResult::Type::Error, tr("Invalid Jellyfin URL."));
  }

  return LoadResult(media_url, LoadResult::Type::TrackAvailable, service_->GetStreamUrl(song_id));

}