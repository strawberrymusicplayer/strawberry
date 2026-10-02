/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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

// Bitrate in kbit/s, the values are also the nicks of the GStreamer Spotify plugin's bitrate property.
enum class Bitrate {
  Bitrate96 = 96,
  Bitrate160 = 160,
  Bitrate320 = 320
};

constexpr char kSettingsGroup[] = "Spotify";

constexpr char kEnabled[] = "enabled";
constexpr char kSearchDelay[] = "searchdelay";
constexpr char kArtistsSearchLimit[] = "artistssearchlimit";
constexpr char kAlbumsSearchLimit[] = "albumssearchlimit";
constexpr char kSongsSearchLimit[] = "songssearchlimit";
constexpr char kFetchAlbums[] = "fetchalbums";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";
constexpr char kRemoveRemastered[] = "remove_remastered";
constexpr char kBitrate[] = "bitrate";

constexpr char kAccessToken[] = "access_token";
constexpr char kRefreshToken[] = "refresh_token";
constexpr char kExpiresIn[] = "expires_in";
constexpr char kLoginTime[] = "login_time";

constexpr char kUseCustomApiCredentials[] = "use_custom_api_credentials";
constexpr char kClientId[] = "client_id";
constexpr char kClientSecret[] = "client_secret";

constexpr bool kDefaultEnabled = false;
constexpr int kDefaultSearchDelay = 1500;
constexpr int kDefaultArtistsSearchLimit = 4;
constexpr int kDefaultAlbumsSearchLimit = 10;
constexpr int kDefaultSongsSearchLimit = 10;
constexpr bool kDefaultFetchAlbums = false;
constexpr bool kDefaultDownloadAlbumCovers = true;
constexpr bool kDefaultRemoveRemastered = true;
constexpr Bitrate kDefaultBitrate = Bitrate::Bitrate320;

// The GStreamer Spotify plugin needs librespot 0.8 to play tracks, which is in version 0.14.4 and newer.
// Older versions fail with "track is not available" for every track.
constexpr int kMinimumGstPluginVersionMajor = 0;
constexpr int kMinimumGstPluginVersionMinor = 14;
constexpr int kMinimumGstPluginVersionMicro = 4;

}  // namespace SpotifySettings

#endif  // SPOTIFYSETTINGS_H
