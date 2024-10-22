/*
* Strawberry Music Player
* Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

constexpr char kAlbum[] = "AlbumEnable";
constexpr char kTechnicalData[] = "TechnicalDataEnable";
constexpr char kSongLyrics[] = "SongLyricsEnable";
constexpr char kSearchCover[] = "SearchCoverEnable";
constexpr char kSearchLyrics[] = "SearchLyricsEnable";

constexpr char kFontHeadline[] = "font_headline";
constexpr char kFontNormal[] = "font_normal";
constexpr char kFontSizeHeadline[] = "font_size_headline";
constexpr char kFontSizeNormal[] = "font_size_normal";

constexpr char kSettingsTitleFmt[] = "TitleFmt";
constexpr char kSettingsSummaryFmt[] = "SummaryFmt";

constexpr char kDefaultFontFamily[] = "Noto Sans";
constexpr qreal kDefaultFontSizeHeadline = 11;

}  // namespace

#endif  // CONTEXTSETTINGS_H
