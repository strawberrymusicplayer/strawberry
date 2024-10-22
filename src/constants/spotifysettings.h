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

#ifndef SPOTIFYSETTINGS_H
#define SPOTIFYSETTINGS_H

namespace SpotifySettings {

constexpr char kSettingsGroup[] = "Spotify";

constexpr char kEnabled[] = "enabled";
constexpr char kSearchDelay[] = "searchdelay";
constexpr char kArtistsSearchLimit[] = "artistssearchlimit";
constexpr char kAlbumsSearchLimit[] = "albumssearchlimit";
constexpr char kSongsSearchLimit[] = "songssearchlimit";
constexpr char kFetchAlbums[] = "fetchalbums";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";

constexpr char kAccessToken[] = "access_token";
constexpr char kRefreshToken[] = "refresh_token";
constexpr char kExpiresIn[] = "expires_in";
constexpr char kLoginTime[] = "login_time";

}  // namespace

#endif  // SPOTIFYSETTINGS_H
