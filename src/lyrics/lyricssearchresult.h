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

#ifndef LYRICSSEARCHRESULT_H
#define LYRICSSEARCHRESULT_H

#include <QMetaType>
#include <QList>
#include <QString>

class LyricsSearchResult {
 public:
  explicit LyricsSearchResult(const QString &_lyrics = QString()) : lyrics(_lyrics), score(0.0) {}
  QString provider;
  QString artist;
  QString album;
  QString title;
  QString lyrics;
  float score;
};
using LyricsSearchResults = QList<LyricsSearchResult>;

Q_DECLARE_METATYPE(LyricsSearchResult)
Q_DECLARE_METATYPE(LyricsSearchResults)

#endif  // LYRICSSEARCHRESULT_H
