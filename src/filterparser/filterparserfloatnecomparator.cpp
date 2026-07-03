/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "filterparserfloatnecomparator.h"

FilterParserFloatNeComparator::FilterParserFloatNeComparator(const float value) : search_term_(value) {}

bool FilterParserFloatNeComparator::Matches(const QVariant &value) const {
  // Quantize both sides (CAST((x + 0.05) * 10 AS INTEGER)) so the in-memory and database rating filters agree, instead of relying on fragile exact float inequality.
  return static_cast<int>((value.toFloat() + 0.05F) * 10.0F) != static_cast<int>((search_term_ + 0.05F) * 10.0F);
}
