/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OPENTIDALSETTINGS_H
#define OPENTIDALSETTINGS_H

namespace OpenTidalSettings {

constexpr char kSettingsGroup[] = "OpenTidal";

constexpr char kEnabled[] = "enabled";
constexpr char kClientId[] = "client_id";
constexpr char kUriScheme[] = "urischeme";
constexpr char kManifestType[] = "manifesttype";
constexpr char kFormat[] = "format";
constexpr char kSearchDelay[] = "searchdelay";
constexpr char kArtistsSearchLimit[] = "artistssearchlimit";
constexpr char kAlbumsSearchLimit[] = "albumssearchlimit";
constexpr char kSongsSearchLimit[] = "songssearchlimit";
constexpr char kFetchAlbums[] = "fetchalbums";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";
constexpr char kCoverSize[] = "coversize";
constexpr char kAlbumExplicit[] = "album_explicit";
constexpr char kRemoveRemastered[] = "remove_remastered";

// The TIDAL Open API delivers playback through track manifests.
// The manifest can be either an MPEG-DASH or a HLS manifest.
enum class ManifestType {
  MPEG_DASH,
  HLS
};

// The manifest URI can either be a HTTPS URL pointing at the manifest,
// or a self-contained data: URI with the manifest embedded inline.
enum class UriScheme {
  HTTPS,
  DATA
};

}  // namespace OpenTidalSettings

#endif  // OPENTIDALSETTINGS_H
