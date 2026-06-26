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

#include "lrcparser.h"

#include <algorithm>

#include <QRegularExpression>
#include <QStringList>

SyncedLyrics LrcParser::Parse(const QString &lrc_text) {

  static const QRegularExpression re(QStringLiteral("^\\[(\\d+):(\\d+\\.?\\d*)\\](.*)$"));

  SyncedLyrics result;
  const QStringList lines = lrc_text.split(QLatin1Char('\n'));

  for (const QString &line : lines) {
    const QRegularExpressionMatch match = re.match(line.trimmed());
    if (!match.hasMatch()) continue;

    const qint64 minutes = match.captured(1).toLongLong();
    const double seconds = match.captured(2).toDouble();
    const QString text = match.captured(3).trimmed();

    if (text.isEmpty()) continue;

    LyricLine lyric_line;
    lyric_line.time_msec = minutes * 60000 + static_cast<qint64>(seconds * 1000.0);
    lyric_line.text = text;
    result.append(lyric_line);
  }

  std::sort(result.begin(), result.end(), [](const LyricLine &a, const LyricLine &b) {
    return a.time_msec < b.time_msec;
  });

  return result;

}
