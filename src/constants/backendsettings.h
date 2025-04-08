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

#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include <QtGlobal>

namespace BackendSettings {

constexpr char kSettingsGroup[] = "Backend";

constexpr char kEngine[] = "engine";
constexpr char kEngineU[] = "Engine";
constexpr char kOutput[] = "output";
constexpr char kOutputU[] = "Output";
constexpr char kDevice[] = "device";
constexpr char kDeviceU[] = "Device";
constexpr char kALSAPlugin[] = "alsaplugin";
constexpr char kPlaybin3[] = "playbin3";
constexpr char kExclusiveMode[] = "exclusive_mode";
constexpr char kVolumeControl[] = "volume_control";
constexpr char kChannelsEnabled[] = "channels_enabled";
constexpr char kChannels[] = "channels";
constexpr char kBS2B[] = "bs2b";
constexpr char kHTTP2[] = "http2";
constexpr char kStrictSSL[] = "strict_ssl";
constexpr char kBufferDuration[] = "bufferduration";
constexpr char kBufferLowWatermark[] = "bufferlowwatermark";
constexpr char kBufferHighWatermark[] = "bufferhighwatermark";
constexpr char kRgEnabled[] = "rgenabled";
constexpr char kRgMode[] = "rgmode";
constexpr char kRgPreamp[] = "rgpreamp";
constexpr char kRgFallbackGain[] = "rgfallbackgain";
constexpr char kRgCompression[] = "rgcompression";
constexpr char kEBUR128LoudnessNormalization[] = "ebur128_loudness_normalization";
constexpr char kEBUR128TargetLevelLUFS[] = "ebur128_target_level_lufs";
constexpr char kFadeoutEnabled[] = "FadeoutEnabled";
constexpr char kCrossfadeEnabled[] = "CrossfadeEnabled";
constexpr char kAutoCrossfadeEnabled[] = "AutoCrossfadeEnabled";
constexpr char kNoCrossfadeSameAlbum[] = "NoCrossfadeSameAlbum";
constexpr char kFadeoutPauseEnabled[] = "FadeoutPauseEnabled";
constexpr char kFadeoutDuration[] = "FadeoutDuration";
constexpr char kFadeoutPauseDuration[] = "FadeoutPauseDuration";

constexpr qint64 kDefaultBufferDuration = 4000;
constexpr double kDefaultBufferLowWatermark = 0.33;
constexpr double kDefaultBufferHighWatermark = 0.99;

}  // namespace

#endif  // BACKENDSETTINGS_H
