/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CONTEXTSETTINGS_H
#define CONTEXTSETTINGS_H

#include <QtGlobal>

namespace ContextSettings {

constexpr char kSettingsGroup[] = "Context";

constexpr char kSettingsTitleFmt[] = "title_fmt";
constexpr char kSettingsSummaryFmt[] = "summary_fmt";

constexpr char kAlbum[] = "album_enable";
constexpr char kTechnicalData[] = "technical_data_enable";
constexpr char kSongLyrics[] = "song_lyrics_enable";
constexpr char kSyncedLyrics[] = "synced_lyrics_enable";
constexpr char kSearchCover[] = "search_cover_enable";
constexpr char kSearchLyrics[] = "search_lyrics_enable";

constexpr char kFontHeadline[] = "font_headline";
constexpr char kFontNormal[] = "font_normal";
constexpr char kFontSizeHeadline[] = "font_size_headline";
constexpr char kFontSizeNormal[] = "font_size_normal";

constexpr char kSyncedLyricsBold[] = "synced_lyrics_bold";
constexpr char kSyncedLyricsItalic[] = "synced_lyrics_italic";
constexpr char kSyncedLyricsSystemColor[] = "synced_lyrics_system_color";
constexpr char kSyncedLyricsColor[] = "synced_lyrics_color";

constexpr char kDefaultTitleFmt[] = "%title% - %artist%";
constexpr char kDefaultSummaryFmt[] = "%album%";

constexpr bool kDefaultAlbum = true;
constexpr bool kDefaultTechnicalData = false;
constexpr bool kDefaultSongLyrics = true;
constexpr bool kDefaultSyncedLyrics = true;
constexpr bool kDefaultSyncedLyricsBold = true;
constexpr bool kDefaultSyncedLyricsItalic = false;
constexpr bool kDefaultSyncedLyricsSystemColor = true;
constexpr bool kDefaultSearchLyrics = true;

constexpr char kDefaultFontFamily[] = "Noto Sans";
constexpr qreal kDefaultFontSizeHeadline = 11;

}  // namespace ContextSettings

#endif  // CONTEXTSETTINGS_H
