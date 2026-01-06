/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "spotifyservice.h"
#include "spotifystreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

SpotifyStreamURLRequest::SpotifyStreamURLRequest(SpotifyService *service, const QUrl &media_url, const uint id, QObject *parent)
    : QObject(parent),
      service_(service),
      media_url_(media_url),
      id_(id),
      song_id_(media_url.path()) {}

void SpotifyStreamURLRequest::Process() {

  if (!service_->authenticated()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Not authenticated with Spotify."));
    return;
  }

  // For Spotify, gst-plugin-spotify handles the spotify: URI directly.
  // We just pass through the same URL as the stream URL.
  // The access token is set separately via the engine.
  Q_EMIT StreamURLSuccess(id_, media_url_, media_url_, Song::FileType::Stream);

}
