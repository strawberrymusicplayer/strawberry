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

#ifndef LYRICSSEARCHTYPE_H
#define LYRICSSEARCHTYPE_H

// What to search for in the lyrics tabs of the song editors.
enum class LyricsSearchType {
  Lyrics = 0,
  SyncedLyrics = 1,
  Both = 2
};

inline bool LyricsSearchTypeHasLyrics(const LyricsSearchType search_type) { return search_type != LyricsSearchType::SyncedLyrics; }
inline bool LyricsSearchTypeHasSyncedLyrics(const LyricsSearchType search_type) { return search_type != LyricsSearchType::Lyrics; }

#endif  // LYRICSSEARCHTYPE_H
