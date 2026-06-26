/*
 * Strawberry Music Player
 * Copyright 2026, guitaripod <guitaripod@users.noreply.github.com>
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

#ifndef LYRICLINE_H
#define LYRICLINE_H

#include <QtGlobal>
#include <QList>
#include <QMetaType>
#include <QString>

struct LyricLine {
  qint64 time_msec;
  QString text;
};
using SyncedLyrics = QList<LyricLine>;

Q_DECLARE_METATYPE(LyricLine)
Q_DECLARE_METATYPE(SyncedLyrics)

#endif  // LYRICLINE_H
