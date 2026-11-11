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

#ifndef ALBUMCOVEREXPORTSETTINGS_H
#define ALBUMCOVEREXPORTSETTINGS_H

namespace AlbumCoverExportSettings {

constexpr char kSettingsGroup[] = "AlbumCoverExport";
constexpr char kFileName[] = "file_name";
constexpr char kOverwrite[] = "overwrite";
constexpr char kForceSize[] = "force_size";
constexpr char kWidth[] = "width";
constexpr char kHeight[] = "height";
constexpr char kExportDownloaded[] = "export_downloaded";
constexpr char kExportEmbedded[] = "export_embedded";

}  // namespace AlbumCoverExportSettings

#endif  // ALBUMCOVEREXPORTSETTINGS_H
