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

#ifndef PLAYLISTSETTINGS_H
#define PLAYLISTSETTINGS_H

#include <QtGlobal>

namespace PlaylistSettings {

constexpr char kSettingsGroup[] = "Playlist";

enum class PathType {
  Automatic = 0,  // Automatically select path type
  Absolute,       // Always use absolute paths
  Relative,       // Always use relative paths
  Ask_User        // Only used in preferences: to ask user which of the previous values he wants to use.
};

constexpr char kAlternatingRowColors[] = "alternating_row_colors";
constexpr char kShowBars[] = "show_bars";
constexpr char kGlowEffect[] = "glow_effect";
constexpr char kWarnClosePlaylist[] = "warn_close_playlist";
constexpr char kContinueOnError[] = "continue_on_error";
constexpr char kGreyoutSongsStartup[] = "greyout_songs_startup";
constexpr char kGreyoutSongsPlay[] = "greyout_songs_play";
constexpr char kSelectTrack[] = "select_track";
constexpr char kShowToolbar[] = "show_toolbar";
constexpr char kPlaylistClear[] = "playlist_clear";
constexpr char kAutoSort[] = "auto_sort";

constexpr char kPathType[] = "path_type";

constexpr char kEditMetadataInline[] = "editmetadatainline";
constexpr char kWriteMetadata[] = "write_metadata";
constexpr char kDeleteFiles[] = "delete_files";

constexpr char kStateVersion[] = "state_version";
constexpr char kState[] = "state";
constexpr char kColumnAlignments[] = "column_alignments";
constexpr char kRatingLocked[] = "rating_locked";

constexpr char kLastSaveFilter[] = "last_save_filter";
constexpr char kLastSavePath[] = "last_save_path";
constexpr char kLastSaveExtension[] = "last_save_extension";

constexpr char kLastSaveAllPath[] = "last_save_all_path";
constexpr char kLastSaveAllExtension[] = "last_save_all_extension";

}  // namespace

Q_DECLARE_METATYPE(PlaylistSettings::PathType)

#endif  // PLAYLISTSETTINGS_H
