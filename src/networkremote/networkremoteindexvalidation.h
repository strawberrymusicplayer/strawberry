/*
 * Strawberry Music Player
 * Copyright 2026, Leopold List <leo@zudiewiener.com>
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

#ifndef NETWORKREMOTEINDEXVALIDATION_H
#define NETWORKREMOTEINDEXVALIDATION_H

#include <QtGlobal>

// Returns true if row_index refers to a valid row within a playlist that
// has row_count rows. Comparing in unsigned space (rather than narrowing
// row_index to int first) avoids a client-supplied value near
// quint32::max() wrapping to a small or negative int and passing a naive
// signed comparison.
inline bool IsValidRowIndex(quint32 row_index, int row_count) {
  if (row_count < 0) return false;
  return row_index < static_cast<quint32>(row_count);
}

#endif  // NETWORKREMOTEINDEXVALIDATION_H
