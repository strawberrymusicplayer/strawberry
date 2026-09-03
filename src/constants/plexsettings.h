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

#ifndef PLEXSETTINGS_H
#define PLEXSETTINGS_H

namespace PlexSettings {

constexpr char kSettingsGroup[] = "Plex";

constexpr char kEnabled[] = "enabled";
constexpr char kServerUrl[] = "server_url";
constexpr char kServerName[] = "server_name";
constexpr char kServerToken[] = "server_token";
constexpr char kServerMachineIdentifier[] = "server_machine_identifier";
constexpr char kToken[] = "token";
constexpr char kClientId[] = "clientid";
constexpr char kVerifyCertificate[] = "verifycertificate";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";
constexpr char kLastUpdate[] = "last_update";

constexpr bool kDefaultEnabled = false;
constexpr bool kDefaultVerifyCertificate = true;
constexpr bool kDefaultDownloadAlbumCovers = true;

}  // namespace PlexSettings

#endif  // PLEXSETTINGS_H
