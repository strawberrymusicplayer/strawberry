/*
* Strawberry Music Player
* Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILENAMECONSTANTS_H
#define FILENAMECONSTANTS_H

#include "includes/arraysize.h"

constexpr char kProblematicCharactersRegex[] = "[:?*\"<>|]";
constexpr char kInvalidFatCharactersRegex[] = "[^a-zA-Z0-9!#\\$%&'()\\-@\\^_`{}~/. ]";
constexpr char kInvalidDirCharactersRegex[] = "[/\\\\]";
constexpr char kInvalidPrefixCharacters[] = ".";
constexpr int kInvalidPrefixCharactersCount = arraysize(kInvalidPrefixCharacters) - 1;

#endif  // FILENAMECONSTANTS_H
