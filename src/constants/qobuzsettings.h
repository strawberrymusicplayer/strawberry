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

#ifndef QOBUZSETTINGS_H
#define QOBUZSETTINGS_H

namespace QobuzSettings {

constexpr char kSettingsGroup[] = "Qobuz";

constexpr char kEnabled[] = "enabled";
constexpr char kAppId[] = "app_id";
constexpr char kAppSecret[] = "app_secret";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";
constexpr char kFormat[] = "format";
constexpr char kSearchDelay[] = "searchdelay";
constexpr char kArtistsSearchLimit[] = "artistssearchlimit";
constexpr char kAlbumsSearchLimit[] = "albumssearchlimit";
constexpr char kSongsSearchLimit[] = "songssearchlimit";
constexpr char kBase64Secret[] = "base64secret";
constexpr char kDownloadAlbumCovers[] = "downloadalbumcovers";

constexpr char kUserId[] = "user_id";
constexpr char kCredentialsId[] = "credentials_id";
constexpr char kDeviceId[] = "device_id";
constexpr char kUserAuthToken[] = "user_auth_token";

}  // namespace

#endif  // QOBUZSETTINGS_H
