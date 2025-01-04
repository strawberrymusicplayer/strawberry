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


#ifndef SUBSONICETTINGS_H
#define SUBSONICETTINGS_H

namespace SubsonicSettings {

constexpr char kSettingsGroup[] = "Subsonic";

enum class AuthMethod {
  Hex,
  MD5
};

constexpr char kEnabled[] = "enabled";
constexpr char kUrl[] = "url";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";
constexpr char kHTTP2[] = "http2";
constexpr char kVerifyCertificate[] = "verifycertificate";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";
constexpr char kUseAlbumIdForAlbumCovers[] = "usealbumidforalbumcovers";
constexpr char kServerSideScrobbling[] = "serversidescrobbling";
constexpr char kAuthMethod[] = "authmethod";

}  // namespace

#endif  // SUBSONICETTINGS_H
