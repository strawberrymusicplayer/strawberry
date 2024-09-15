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

#ifndef SAVETAGSOPTIONS_H
#define SAVETAGSOPTIONS_H

#include <QtGlobal>

enum class SaveTagsOption {
  NoType = 0,
  Tags = 1,
  Playcount = 2,
  Rating = 4,
  Cover = 8
};
Q_DECLARE_FLAGS(SaveTagsOptions, SaveTagsOption)

#endif  // SAVETAGSOPTIONS_H
