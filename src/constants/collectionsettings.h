/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONSETTINGS_H
#define COLLECTIONSETTINGS_H

namespace CollectionSettings {

constexpr char kSettingsGroup[] = "Collection";

enum class CacheSizeUnit {
  KB,
  MB,
  GB,
  TB
};

constexpr char kStartupScan[] = "startup_scan";
constexpr char kMonitor[] = "monitor";
constexpr char kSongTracking[] = "song_tracking";
constexpr char kMarkSongsUnavailable[] = "mark_songs_unavailable";
constexpr char kSongENUR128LoudnessAnalysis[] = "song_ebur128_loudness_analysis";
constexpr char kExpireUnavailableSongs[] = "expire_unavailable_songs";
constexpr char kCoverArtPatterns[] = "cover_art_patterns";
constexpr char kAutoOpen[] = "auto_open";
constexpr char kShowDividers[] = "show_dividers";
constexpr char kPrettyCovers[] = "pretty_covers";
constexpr char kVariousArtists[] = "various_artists";
constexpr char kSkipArticlesForArtists[] = "skip_articles_for_artists";
constexpr char kSkipArticlesForAlbums[] = "skip_articles_for_albums";
constexpr char kUseSortTags[] = "use_sort_tags";
constexpr char kSettingsCacheSize[] = "cache_size";
constexpr char kSettingsCacheSizeUnit[] = "cache_size_unit";
constexpr char kSettingsDiskCacheEnable[] = "disk_cache_enable";
constexpr char kSettingsDiskCacheSize[] = "disk_cache_size";
constexpr char kSettingsDiskCacheSizeUnit[] = "disk_cache_size_unit";
constexpr int kSettingsCacheSizeDefault = 160;
constexpr int kSettingsDiskCacheSizeDefault = 360;
constexpr char kSavePlayCounts[] = "save_playcounts";
constexpr char kSaveRatings[] = "save_ratings";
constexpr char kOverwritePlaycount[] = "overwrite_playcount";
constexpr char kOverwriteRating[] = "overwrite_rating";
constexpr char kDeleteFiles[] = "delete_files";
constexpr char kLastPath[] = "last_path";

constexpr bool kDefaultStartupScan = true;
constexpr bool kDefaultMonitor = true;
constexpr bool kDefaultSongTracking = false;
constexpr bool kDefaultMarkSongsUnavailable = true;
constexpr bool kDefaultSongENUR128LoudnessAnalysis = false;
constexpr int kDefaultExpireUnavailableSongs = 60;
constexpr bool kDefaultAutoOpen = false;
constexpr bool kDefaultShowDividers = true;
constexpr bool kDefaultPrettyCovers = true;
constexpr bool kDefaultVariousArtists = true;
constexpr bool kDefaultSkipArticlesForArtists = true;
constexpr bool kDefaultSkipArticlesForAlbums = false;
constexpr bool kDefaultUseSortTags = true;
constexpr CacheSizeUnit kDefaultSettingsCacheSizeUnit = CacheSizeUnit::MB;
constexpr bool kDefaultSettingsDiskCacheEnable = false;
constexpr CacheSizeUnit kDefaultSettingsDiskCacheSizeUnit = CacheSizeUnit::MB;
constexpr bool kDefaultSavePlayCounts = false;
constexpr bool kDefaultSaveRatings = false;
constexpr bool kDefaultOverwritePlaycount = false;
constexpr bool kDefaultOverwriteRating = false;
constexpr bool kDefaultDeleteFiles = false;

}  // namespace CollectionSettings

#endif  // COLLECTIONSETTINGS_H
