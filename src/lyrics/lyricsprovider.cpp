/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QObject>
#include <QString>
#include <QRegularExpression>

#include "core/utilities.h"
#include "core/networkaccessmanager.h"
#include "lyricsprovider.h"

LyricsProvider::LyricsProvider(const QString &name, const bool enabled, const bool authentication_required, NetworkAccessManager *network, QObject *parent)
    : QObject(parent), network_(network), name_(name), enabled_(enabled), order_(0), authentication_required_(authentication_required) {}

QString LyricsProvider::ParseLyricsFromHTML(const QString &content, const QRegularExpression &start_tag, const QRegularExpression &end_tag, const QRegularExpression &lyrics_start) {

  QRegularExpressionMatch rematch = lyrics_start.match(content);
  if (!rematch.hasMatch()) return QString();

  const qint64 start_lyrics_tag_idx = rematch.capturedEnd();

  // Find the index of the end tag.
  qint64 start_tag_idx = 0;
  qint64 end_tag_idx = 0;
  qint64 idx = start_lyrics_tag_idx;
  int tags = 1;
  do {
    QRegularExpressionMatch rematch_start_tag = QRegularExpression(start_tag).match(content, idx);
    start_tag_idx = rematch_start_tag.hasMatch() ? rematch_start_tag.capturedStart() : -1;
    QRegularExpressionMatch rematch_end_tag = QRegularExpression(end_tag).match(content, idx);
    end_tag_idx = rematch_end_tag.hasMatch() ? rematch_end_tag.capturedStart() : -1;
    if (rematch_start_tag.hasMatch() && start_tag_idx <= end_tag_idx) {
      ++tags;
      idx = start_tag_idx + rematch_start_tag.capturedLength();
    }
    else if (rematch_end_tag.hasMatch()) {
      --tags;
      idx = end_tag_idx + rematch_end_tag.capturedLength();
    }
  }
  while (tags > 0 || end_tag_idx >= start_tag_idx);

  if (end_tag_idx <= 0 || start_lyrics_tag_idx >= end_tag_idx) return QString();

  QString lyrics = content.mid(start_lyrics_tag_idx, end_tag_idx - start_lyrics_tag_idx)
                          .replace(QRegularExpression("<br[^>]+>"), "\n")
                          .remove(QRegularExpression("<[^>]*>"))
                          .trimmed();

  if (lyrics.length() > 6000 || lyrics.contains("there are no lyrics to", Qt::CaseInsensitive)) {
    return QString();
  }

  return Utilities::DecodeHtmlEntities(lyrics);
  
}

