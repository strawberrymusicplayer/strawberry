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

#ifndef BACKENDSETTINGS_H
#define BACKENDSETTINGS_H

#include <QtGlobal>

namespace BackendSettings {

constexpr char kSettingsGroup[] = "Backend";

constexpr char kOutput[] = "output";
constexpr char kDevice[] = "device";
constexpr char kALSAPlugin[] = "alsa_plugin";
constexpr char kExclusiveMode[] = "exclusive_mode";
constexpr char kVolumeControl[] = "volume_control";
constexpr char kVolumeExponential[] = "volume_exponential";
constexpr char kChannelsEnabled[] = "channels_enabled";
constexpr char kChannels[] = "channels";
constexpr char kBS2B[] = "bs2b";
constexpr char kPlaybin3[] = "playbin3";
constexpr char kHTTP2[] = "http2";
constexpr char kStrictSSL[] = "strict_ssl";
constexpr char kBufferDuration[] = "buffer_duration";
constexpr char kBufferLowWatermark[] = "buffer_low_watermark";
constexpr char kBufferHighWatermark[] = "buffer_high_watermark";
constexpr char kDeviceWarmupDuration[] = "device_warmup_duration";
constexpr char kRgEnabled[] = "rg_enabled";
constexpr char kRgMode[] = "rg_mode";
constexpr char kRgPreamp[] = "rg_preamp";
constexpr char kRgCompression[] = "rg_compression";
constexpr char kRgFallbackGain[] = "rg_fallback_gain";
constexpr char kEBUR128LoudnessNormalization[] = "ebur128_loudness_normalization";
constexpr char kEBUR128TargetLevelLUFS[] = "ebur128_target_level_lufs";
constexpr char kFadeoutEnabled[] = "fadeout_enabled";
constexpr char kCrossfadeEnabled[] = "crossfade_enabled";
constexpr char kAutoCrossfadeEnabled[] = "auto_crossfade_enabled";
constexpr char kNoCrossfadeSameAlbum[] = "no_crossfade_same_album";
constexpr char kFadeoutPauseEnabled[] = "fadeout_pause_enabled";
constexpr char kFadeoutDuration[] = "fadeout_duration";
constexpr char kFadeoutPauseDuration[] = "fadeout_pause_duration";

constexpr bool kDefaultExclusiveMode = false;
constexpr bool kDefaultVolumeControl = true;
constexpr bool kDefaultVolumeExponential = false;
constexpr bool kDefaultChannelsEnabled = false;
constexpr int kDefaultChannels = 0;
constexpr bool kDefaultBS2B = false;
constexpr bool kDefaultPlaybin3 = true;
constexpr bool kDefaultHTTP2 = false;
constexpr bool kDefaultStrictSSL = false;
constexpr qint64 kDefaultBufferDuration = 4000LL;
constexpr double kDefaultBufferLowWatermark = 0.33;
constexpr double kDefaultBufferHighWatermark = 0.99;
constexpr int kDefaultDeviceWarmupDuration = 500;
constexpr bool kDefaultRgEnabled = false;
constexpr int kDefaultRgMode = 0;
constexpr double kDefaultRgPreamp = 0.0;
constexpr bool kDefaultRgCompression = true;
constexpr double kDefaultRgFallbackGain = 0.0;
constexpr bool kDefaultEBUR128LoudnessNormalization = false;
constexpr double kDefaultEBUR128TargetLevelLUFS = -23.0;
constexpr bool kDefaultFadeoutEnabled = false;
constexpr bool kDefaultCrossfadeEnabled = false;
constexpr bool kDefaultAutoCrossfadeEnabled = false;
constexpr bool kDefaultNoCrossfadeSameAlbum = true;
constexpr bool kDefaultFadeoutPauseEnabled = false;
constexpr qint64 kDefaultFadeoutDuration = 2000LL;
constexpr qint64 kDefaultFadeoutPauseDuration = 250LL;

}  // namespace BackendSettings

#endif  // BACKENDSETTINGS_H
