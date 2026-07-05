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

enum class StreamUrlMethod {
  StreamUrl,
  UrlPostPaywall,
  PlaybackInfoPostPaywall
};

constexpr char kSettingsGroup[] = "Tidal";

constexpr char kEnabled[] = "enabled";
constexpr char kClientId[] = "client_id";
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
constexpr char kRemoveRemastered[] = "remove_remastered";

constexpr char kOAuth[] = "oauth";
constexpr char kApiToken[] = "api_token";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";

constexpr bool kDefaultEnabled = false;
constexpr char kDefaultQuality[] = "LOSSLESS";
constexpr int kDefaultSearchDelay = 1500;
constexpr int kDefaultArtistsSearchLimit = 4;
constexpr int kDefaultAlbumsSearchLimit = 10;
constexpr int kDefaultSongsSearchLimit = 10;
constexpr bool kDefaultFetchAlbums = false;
constexpr bool kDefaultDownloadAlbumCovers = true;
constexpr char kDefaultCoverSize[] = "640x640";
constexpr StreamUrlMethod kDefaultStreamUrl = StreamUrlMethod::StreamUrl;
constexpr bool kDefaultAlbumExplicit = false;
constexpr bool kDefaultRemoveRemastered = true;

}  // namespace TidalSettings

#endif  // TIDALSETTINGS_H
