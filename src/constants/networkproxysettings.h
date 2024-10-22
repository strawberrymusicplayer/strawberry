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

#ifndef NETWORKPROXYSETTINGS_H
#define NETWORKPROXYSETTINGS_H

namespace NetworkProxySettings {

constexpr char kSettingsGroup[] = "NetworkProxy";

constexpr char kType[] = "type";
constexpr char kHostname[] = "hostname";
constexpr char kPort[] = "port";
constexpr char kUseAuthentication[] = "use_authentication";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";
constexpr char kEngine[] = "engine";

}  // namespace

#endif  // NETWORKPROXYSETTINGS_H
