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

#ifndef NETWORKREMOTESETTINGSCONSTANTS_H
#define NETWORKREMOTESETTINGSCONSTANTS_H

namespace NetworkRemoteSettingsConstants {

constexpr char kSettingsGroup[] = "NetworkRemote";
constexpr char kEnabled[] = "enabled";
constexpr char kPort[] = "port";
constexpr char kAllowPublicAccess[] = "allow_public_access";
constexpr char kUseAuthCode[] = "use_authcode";
constexpr char kAuthCode[] = "authcode";
constexpr char kFilesRootFolder[] = "files_root_folder";

}  // namespace NetworkRemoteSettingsConstants

#endif // NETWORKREMOTESETTINGSCONSTANTS_H
