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

#ifndef NOTIFICATIONSSETTINGS_H
#define NOTIFICATIONSSETTINGS_H

#include <QRgb>

namespace OSDSettings {

constexpr char kSettingsGroup[] = "OSD";

constexpr char kType[] = "Behaviour";

enum class Type {
  Disabled = 0,
  Native,
  TrayPopup,
  Pretty
};

constexpr char kTimeout[] = "Timeout";
constexpr char kShowOnVolumeChange[] = "ShowOnVolumeChange";
constexpr char kShowOnPlayModeChange[] = "ShowOnPlayModeChange";
constexpr char kShowOnPausePlayback[] = "ShowOnPausePlayback";
constexpr char kShowOnResumePlayback[] = "ShowOnResumePlayback";
constexpr char kShowArt[] = "ShowArt";
constexpr char kCustomTextEnabled[] = "CustomTextEnabled";
constexpr char kCustomText1[] = "CustomText1";
constexpr char kCustomText2[] = "CustomText2";

}  // namespace

namespace OSDPrettySettings {

constexpr char kSettingsGroup[] = "OSDPretty";

constexpr char kForegroundColor[] = "foreground_color";
constexpr char kBackgroundColor[] = "background_color";
constexpr char kBackgroundOpacity[] = "background_opacity";
constexpr char kPopupScreen[] = "popup_screen";
constexpr char kPopupPos[] = "popup_pos";
constexpr char kFont[] = "font";
constexpr char kDisableDuration[] = "disable_duration";
constexpr char kFading[] = "fading";

constexpr QRgb kPresetBlue = qRgb(102, 150, 227);
constexpr QRgb kPresetRed = qRgb(202, 22, 16);

}  // namespace

namespace DiscordRPCSettings {

constexpr char kSettingsGroup[] = "DiscordRPC";

constexpr char kEnabled[] = "enabled";

constexpr char kStatusDisplayType[] = "StatusDisplayType";

enum class StatusDisplayType {
  App = 0,
  Artist,
  Song
};

}  // namespace

#endif  // NOTIFICATIONSSETTINGS_H
