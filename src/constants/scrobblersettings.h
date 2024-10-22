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

#ifndef SCROBBLERSETTINGS_H
#define SCROBBLERSETTINGS_H

namespace ScrobblerSettings {

constexpr char kSettingsGroup[] = "Scrobbler";

constexpr char kEnabled[] = "enabled";
constexpr char kScrobbleButton[] = "scrobble_button";
constexpr char kLoveButton[] = "love_button";
constexpr char kOffline[] = "offline";
constexpr char kSubmit[] = "submit";
constexpr char kAlbumArtist[] = "albumartist";
constexpr char kShowErrorDialog[] = "show_error_dialog";
constexpr char kStripRemastered[] = "strip_remastered";
constexpr char kSources[] = "sources";
constexpr char kUserToken[] = "user_token";

}  // namespace

#endif  // SCROBBLERSETTINGS_H
