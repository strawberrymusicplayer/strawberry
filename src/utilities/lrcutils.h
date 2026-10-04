/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LRCUTILS_H
#define LRCUTILS_H

#include <optional>

#include <QtGlobal>
#include <QList>
#include <QString>

namespace Utilities {

struct LRCLine {
  uint time;  // Absolute time in milliseconds.
  QString text;
};
using LRCLines = QList<LRCLine>;

// Parses LRC into lines sorted by time, with lines that have multiple timestamps expanded and the offset tag applied.
// Returns an empty list for empty input, and std::nullopt if the LRC is invalid.
std::optional<LRCLines> ParseLRC(const QString &lrc);

}  // namespace Utilities

#endif  // LRCUTILS_H
