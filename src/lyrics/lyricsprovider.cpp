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

#include "utilities/strutils.h"
#include "core/networkaccessmanager.h"
#include "lyricsprovider.h"

LyricsProvider::LyricsProvider(const QString &name, const bool enabled, const bool authentication_required, NetworkAccessManager *network, QObject *parent)
    : QObject(parent), network_(network), name_(name), enabled_(enabled), order_(0), authentication_required_(authentication_required) {}

QString LyricsProvider::ParseLyricsFromHTML(const QString &content, const QRegularExpression &start_tag, const QRegularExpression &end_tag, const QRegularExpression &lyrics_start, const bool multiple) {

  QString lyrics;
  qint64 start_idx = 0;

  do {

    QRegularExpressionMatch rematch = lyrics_start.match(content, start_idx);
    if (!rematch.hasMatch()) break;

    const qint64 start_lyrics_idx = rematch.capturedEnd();
    qint64 end_lyrics_idx = -1;

    // Find the index of the end tag.
    qint64 idx = start_lyrics_idx;
    QRegularExpressionMatch rematch_start_tag;
    QRegularExpressionMatch rematch_end_tag;
    int tags = 1;
    do {
      rematch_start_tag = QRegularExpression(start_tag).match(content, idx);
      const qint64 start_tag_idx = rematch_start_tag.hasMatch() ? rematch_start_tag.capturedStart() : -1;
      rematch_end_tag = QRegularExpression(end_tag).match(content, idx);
      const qint64 end_tag_idx = rematch_end_tag.hasMatch() ? rematch_end_tag.capturedStart() : -1;
      if (rematch_start_tag.hasMatch() && start_tag_idx <= end_tag_idx) {
        ++tags;
        idx = start_tag_idx + rematch_start_tag.capturedLength();
      }
      else if (rematch_end_tag.hasMatch()) {
        --tags;
        idx = end_tag_idx + rematch_end_tag.capturedLength();
        if (tags == 0) {
          end_lyrics_idx = rematch_end_tag.capturedStart();
          start_idx = rematch_end_tag.capturedEnd();
        }
      }
    }
    while (tags > 0 && (rematch_start_tag.hasMatch() || rematch_end_tag.hasMatch()));

    if (end_lyrics_idx != -1 && start_lyrics_idx < end_lyrics_idx) {
      if (!lyrics.isEmpty()) {
        lyrics.append("\n");
      }
      lyrics.append(content.mid(start_lyrics_idx, end_lyrics_idx - start_lyrics_idx)
                           .replace(QRegularExpression("<br[^>]*>"), "\n")
                           .remove(QRegularExpression("<[^>]*>"))
                           .trimmed());
    }
    else {
      start_idx = -1;
    }

  }
  while (start_idx > 0 && multiple);

  if (lyrics.length() > 6000 || lyrics.contains("there are no lyrics to", Qt::CaseInsensitive)) {
    return QString();
  }

  return Utilities::DecodeHtmlEntities(lyrics);

}
