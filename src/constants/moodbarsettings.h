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

#ifndef MOODBARSETTINGS_H
#define MOODBARSETTINGS_H

namespace MoodbarSettings {

constexpr char kSettingsGroup[] = "Moodbar";

enum class Style {
  Normal = 0,
  Angry,
  Frozen,
  Happy,
  SystemPalette,
  StyleCount
};

constexpr char kEnabled[] = "enabled";
constexpr char kShow[] = "show";
constexpr char kStyle[] = "style";
constexpr char kSave[] = "save";

}  // namespace

#endif  // MOODBARSETTINGS_H
