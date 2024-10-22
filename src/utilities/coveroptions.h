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

#ifndef COVEROPTIONS_H
#define COVEROPTIONS_H

#include <QString>

class CoverOptions {
 public:

  enum class CoverType {
    Cache = 1,
    Album = 2,
    Embedded = 3
  };

  enum class CoverFilename {
    Hash = 1,
    Pattern = 2
  };

  explicit CoverOptions();
  CoverType cover_type;
  CoverFilename cover_filename;
  QString cover_pattern;
  bool cover_overwrite;
  bool cover_lowercase;
  bool cover_replace_spaces;
};

#endif  // COVEROPTIONS_H
