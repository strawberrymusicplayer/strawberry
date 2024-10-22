/*
* Strawberry Music Player
* Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TIDALSETTINGS_H
#define TIDALSETTINGS_H

namespace TidalSettings {

constexpr char kSettingsGroup[] = "Tidal";

constexpr char kEnabled[] = "enabled";
constexpr char kOAuth[] = "oauth";
constexpr char kClientId[] = "client_id";
constexpr char kApiToken[] = "api_token";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";
constexpr char kQuality[] = "quality";
constexpr char kSearchDelay[] = "searchdelay";
constexpr char kArtistsSearchLimit[] = "artistssearchlimit";
constexpr char kAlbumsSearchLimit[] = "albumssearchlimit";
constexpr char kSongsSearchLimit[] = "songssearchlimit";
constexpr char kFetchAlbums[] = "fetchalbums";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";
constexpr char kCoverSize[] = "coversize";
constexpr char kStreamUrl[] = "streamurl";
constexpr char kAlbumExplicit[] = "album_explicit";

enum class StreamUrlMethod {
  StreamUrl,
  UrlPostPaywall,
  PlaybackInfoPostPaywall
};

}

#endif  // TIDALSETTINGS_H
