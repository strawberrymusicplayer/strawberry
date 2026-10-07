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

#include <optional>
#include <limits>
#include <algorithm>
#include <utility>

#include <QList>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

#include "lrcutils.h"

namespace Utilities {

using namespace Qt::Literals::StringLiterals;

std::optional<LRCLines> ParseLRC(const QString &lrc) {

  // LRC lines can have multiple leading timestamps, e.g. "[00:12.34][01:23.45]Chorus line".
  // Blank lines and LRC metadata tags such as "[ar:Artist]" are skipped, any other line without a timestamp makes the LRC invalid.
  static const QRegularExpression regex_timestamp(u"^\\[(\\d+):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"_s);
  static const QRegularExpression regex_offset(u"^\\[offset:\\s*([+-]?\\d+)\\s*\\]$"_s, QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression regex_metadata(u"^\\[[A-Za-z#]+:[^\\]]*\\]$"_s);

  // Lines with multiple timestamps are duplicated for each of them, limit how much that can grow the text so crafted input cannot exhaust memory.
  constexpr qsizetype kMaxExpandedSize = 1024 * 1024;
  const qsizetype max_text_size = lrc.size() + kMaxExpandedSize;
  qsizetype text_size = 0;

  // Times are limited to 32-bit milliseconds as used by ID3v2 SYLT, they are kept as 64-bit until the offset is applied so overflow can be detected.
  constexpr qint64 kMaxTime = std::numeric_limits<uint>::max();

  qint64 offset = 0;
  QList<std::pair<qint64, QString>> entries;
  // Lines can end with CR LF, LF or only CR (classic Mac OS).
  static const QRegularExpression regex_line_break(u"\\r\\n|\\n|\\r"_s);
  const QStringList lines = lrc.split(regex_line_break);
  for (const QString &line : lines) {
    QString text = line;
    QList<qint64> times;
    QRegularExpressionMatch match = regex_timestamp.match(text);
    while (match.hasMatch()) {
      bool minutes_ok = false;
      const qint64 minutes = match.captured(1).toLongLong(&minutes_ok);
      const qint64 seconds = match.captured(2).toLongLong();
      // Fraction is in tenths, hundredths or thousandths depending on the number of digits.
      const qint64 milliseconds = match.captured(3).leftJustified(3, u'0').toLongLong();
      if (!minutes_ok || minutes > kMaxTime / 60000 || seconds >= 60) {
        return std::nullopt;
      }
      const qint64 time = (minutes * 60000) + (seconds * 1000) + milliseconds;
      if (time > kMaxTime) {
        return std::nullopt;
      }
      times << time;
      text.remove(0, match.capturedLength(0));
      match = regex_timestamp.match(text);
    }
    if (times.isEmpty()) {
      const QString trimmed = text.trimmed();
      if (trimmed.isEmpty()) continue;
      // A positive offset makes the lyrics appear sooner, it is applied to the timestamps so they are absolute.
      // Check for the tag name separately, so a malformed offset is rejected instead of being skipped as generic metadata.
      if (trimmed.startsWith("[offset:"_L1, Qt::CaseInsensitive)) {
        const QRegularExpressionMatch match_offset = regex_offset.match(trimmed);
        if (!match_offset.hasMatch()) {
          return std::nullopt;
        }
        bool offset_ok = false;
        offset = match_offset.captured(1).toLongLong(&offset_ok);
        if (!offset_ok || offset > kMaxTime || offset < -kMaxTime) {
          return std::nullopt;
        }
        continue;
      }
      if (regex_metadata.match(trimmed).hasMatch()) continue;
      return std::nullopt;
    }
    text_size += text.size() * times.size();
    if (text_size > max_text_size) {
      return std::nullopt;
    }
    for (const qint64 time : std::as_const(times)) {
      entries << std::make_pair(time, text);
    }
  }

  // Non-empty LRC without any timed lines would clear the lyrics, treat it as invalid instead.
  if (entries.isEmpty() && !lrc.trimmed().isEmpty()) {
    return std::nullopt;
  }

  for (std::pair<qint64, QString> &entry : entries) {
    // Lines shifted before the start of the track are shown at the start instead.
    entry.first = std::max(entry.first - offset, static_cast<qint64>(0));
    if (entry.first > kMaxTime) {
      return std::nullopt;
    }
  }

  std::stable_sort(entries.begin(), entries.end(), [](const std::pair<qint64, QString> &a, const std::pair<qint64, QString> &b) { return a.first < b.first; });

  LRCLines lrc_lines;
  lrc_lines.reserve(entries.size());
  for (const std::pair<qint64, QString> &entry : std::as_const(entries)) {
    lrc_lines << LRCLine{ static_cast<uint>(entry.first), entry.second };
  }

  return lrc_lines;

}

QString LRCFilename(const QString &media_filename) {

  // The collection watcher matches the extension case insensitively, so look for an existing file with the extension in any case, otherwise a second file could be created next to it.
  const QFileInfo fileinfo(media_filename);
  const QString complete_base_name = fileinfo.completeBaseName();
  QDirIterator it(fileinfo.path(), QDir::Files | QDir::Hidden);
  while (it.hasNext()) {
    const QFileInfo lrc_fileinfo(it.next());
    if (lrc_fileinfo.completeBaseName() == complete_base_name && lrc_fileinfo.suffix().compare("lrc"_L1, Qt::CaseInsensitive) == 0) {
      return lrc_fileinfo.filePath();
    }
  }

  return fileinfo.path() + u'/' + complete_base_name + u".lrc"_s;

}

}  // namespace Utilities
