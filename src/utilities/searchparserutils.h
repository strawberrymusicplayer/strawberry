/*
 * Strawberry Music Player
 * Copyright 2019-2023, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2023, Daniel Ostertag <daniel.ostertag@dakes.de>
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

#ifndef SEARCHPARSERUTILS_H
#define SEARCHPARSERUTILS_H

#include <QString>

namespace Utilities {

int ParseSearchTime(const QString &time_str);
float ParseSearchRating(const QString &rating_str);

}  // namespace Utilities

#endif  // SEARCHPARSERUTILS_H
