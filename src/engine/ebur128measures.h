/*
 * Strawberry Music Player
 * Copyright 2023 Roman Lebedev
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

#ifndef EBUR128MEASURES_H
#define EBUR128MEASURES_H

#include <optional>

struct EBUR128Measures {
  std::optional<double> loudness_lufs;  // Global integrated loudness
  std::optional<double> range_lu;       // Loudness Range
};

#endif  // EBUR128MEASURES_H
