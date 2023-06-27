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

#ifndef EBUR128ANALYSIS_H
#define EBUR128ANALYSIS_H

#include "config.h"

#include <optional>

#include "core/song.h"
#include "ebur128measures.h"

class EBUR128Analysis {
 public:
  ~EBUR128Analysis() = delete;  // Do not construct variables of this class.

  // Performs an EBU R 128 analysis on the given song.
  // Returns `std::nullopt` if the analysis fails.
  //
  // This method is blocking, so you want to call it in another thread.
  static std::optional<EBUR128Measures> Compute(const Song &song);
};

#endif  // EBUR128ANALYSIS_H
