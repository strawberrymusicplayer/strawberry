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

#ifndef APPEARANCESETTINGS_H
#define APPEARANCESETTINGS_H

namespace AppearanceSettings {

constexpr char kSettingsGroup[] = "Appearance";

constexpr char kStyle[] = "style";
constexpr char kSystemThemeIcons[] = "system_icons";

constexpr char kBackgroundImageType[] = "background_image_type";
constexpr char kBackgroundImageFilename[] = "background_image_file";
constexpr char kBackgroundImagePosition[] = "background_image_position";
constexpr char kBackgroundImageStretch[] = "background_image_stretch";
constexpr char kBackgroundImageDoNotCut[] = "background_image_do_not_cut";
constexpr char kBackgroundImageKeepAspectRatio[] = "background_image_keep_aspect_ratio";
constexpr char kBackgroundImageMaxSize[] = "background_image_max_size";

constexpr char kBlurRadius[] = "blur_radius";
constexpr char kOpacityLevel[] = "opacity_level";

constexpr int kDefaultBlurRadius = 0;
constexpr int kDefaultOpacityLevel = 40;

constexpr char kTabBarSystemColor[] = "tab_system_color";
constexpr char kTabBarGradient[] = "tab_gradient";
constexpr char kTabBarColor[] = "tab_color";

constexpr char kIconSizeTabbarSmallMode[] = "icon_size_tabbar_small_mode";
constexpr char kIconSizeTabbarLargeMode[] = "icon_size_tabbar_large_mode";
constexpr char kIconSizePlayControlButtons[] = "icon_size_play_control_buttons";
constexpr char kIconSizePlaylistButtons[] = "icon_size_playlist_buttons";
constexpr char kIconSizeLeftPanelButtons[] = "icon_size_left_panel_buttons";
constexpr char kIconSizeConfigureButtons[] = "icon_size_configure_buttons";

constexpr char kPlaylistPlayingSongColor[] = "playlist_playing_song_color";

enum class BackgroundImageType {
  Default,
  None,
  Custom,
  Album,
  Strawbs
};

enum class BackgroundImagePosition {
  UpperLeft = 1,
  UpperRight = 2,
  Middle = 3,
  BottomLeft = 4,
  BottomRight = 5
};

}  // namespace

#endif  // APPEARANCESETTINGS_H
