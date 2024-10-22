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

#ifndef COVERSSETTINGS_H
#define COVERSSETTINGS_H

namespace CoversSettings {

constexpr char kSettingsGroup[] = "Covers";
constexpr char kProviders[] = "providers";
constexpr char kTypes[] = "types";
constexpr char kSaveType[] = "save_type";
constexpr char kSaveFilename[] = "save_filename";
constexpr char kSavePattern[] = "save_pattern";
constexpr char kSaveOverwrite[] = "save_overwrite";
constexpr char kSaveLowercase[] = "save_lowercase";
constexpr char kSaveReplaceSpaces[] = "save_replace_spaces";

}  // namespace

#endif  // COVERSSETTINGS_H
