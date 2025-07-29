/*
 * Strawberry Music Player
 * This file was part of Amarok / Clementine
 * Copyright 2003 Mark Kretschmann
 * Copyright 2004 - 2005 Max Howell, <max.howell@methylblue.com>
 * Copyright 2010 David Sansome <me@davidsansome.com>
 * Copyright 2017-2021 Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <cmath>

#include <QtGlobal>
#include <QVariant>
#include <QUrl>
#include <QSettings>

#include "utilities/envutils.h"
#include "constants/timeconstants.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/networkproxyfactory.h"
#include "enginebase.h"
#include "constants/backendsettings.h"
#include "constants/networkproxysettings.h"
#ifdef HAVE_SPOTIFY
#  include "constants/spotifysettings.h"
#endif

using namespace Qt::Literals::StringLiterals;

EngineBase::EngineBase(QObject *parent)
    : QObject(parent),
      playbin3_enabled_(true),
      exclusive_mode_(false),
      volume_control_(true),
      volume_(100),
      beginning_offset_nanosec_(0),
      end_offset_nanosec_(0),
      ebur128_loudness_normalizing_gain_db_(0.0),
      scope_(kScopeSize),
      buffering_(false),
      equalizer_enabled_(false),
      rg_enabled_(false),
      rg_mode_(0),
      rg_preamp_(0.0),
      rg_fallbackgain_(0.0),
      rg_compression_(true),
      ebur128_loudness_normalization_(false),
      ebur128_target_level_lufs_(-23.0),
      buffer_duration_nanosec_(BackendSettings::kDefaultBufferDuration * kNsecPerMsec),
      buffer_low_watermark_(BackendSettings::kDefaultBufferLowWatermark),
      buffer_high_watermark_(BackendSettings::kDefaultBufferHighWatermark),
      fadeout_enabled_(true),
      crossfade_enabled_(true),
      autocrossfade_enabled_(false),
      crossfade_same_album_(false),
      fadeout_pause_enabled_(false),
      fadeout_duration_(2),
      fadeout_duration_nanosec_(2 * kNsecPerSec),
      fadeout_pause_duration_(0),
      fadeout_pause_duration_nanosec_(0),
      proxy_authentication_(false),
      channels_enabled_(false),
      channels_(0),
      bs2b_enabled_(false),
      http2_enabled_(true),
      strict_ssl_enabled_(false),
      about_to_end_emitted_(false) {}

EngineBase::~EngineBase() = default;

bool EngineBase::Load(const QUrl &media_url, const QUrl &stream_url, const TrackChangeFlags track_change_flags, const bool force_stop_at_end, const quint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  Q_UNUSED(track_change_flags)
  Q_UNUSED(force_stop_at_end);

  media_url_ = media_url;
  stream_url_ = stream_url;
  beginning_offset_nanosec_ = beginning_offset_nanosec;
  end_offset_nanosec_ = end_offset_nanosec;

  ebur128_loudness_normalizing_gain_db_ = 0.0;
  if (ebur128_loudness_normalization_ && ebur128_integrated_loudness_lufs) {
    auto computeGain_dB = [](double source_dB, double target_dB) {
      // Let's suppose the `source_dB` is -12 dB, while `target_dB` is -23 dB.
      // In that case, we'd need to apply -11 dB of gain, which is computed as:
      //   -12 dB + x dB = -23 dB --> x dB = -23 dB - (-12 dB)
      return target_dB - source_dB;
    };

    ebur128_loudness_normalizing_gain_db_ = computeGain_dB(*ebur128_integrated_loudness_lufs, ebur128_target_level_lufs_);
  }

  about_to_end_emitted_ = false;

  return true;

}

bool EngineBase::Play(const QUrl &media_url, const QUrl &stream_url, const bool pause, const TrackChangeFlags flags, const bool force_stop_at_end, const quint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const quint64 offset_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  if (!Load(media_url, stream_url, flags, force_stop_at_end, beginning_offset_nanosec, end_offset_nanosec, ebur128_integrated_loudness_lufs)) {
    return false;
  }

  return Play(pause, offset_nanosec);

}

void EngineBase::UpdateVolume(const uint volume) {

  volume_ = volume;
  Q_EMIT VolumeChanged(volume);

}

void EngineBase::SetVolume(const uint volume) {

  volume_ = volume;
  SetVolumeSW(volume);

}

void EngineBase::ReloadSettings() {

  Settings s;

  s.beginGroup(BackendSettings::kSettingsGroup);

  if (s.contains(BackendSettings::kOutputU)) {
    output_ = s.value(BackendSettings::kOutputU).toString();
  }
  else if (s.contains(BackendSettings::kOutput)) {
    output_ = s.value(BackendSettings::kOutput).toString();
  }

  if (s.contains(BackendSettings::kDeviceU)) {
    device_ = s.value(BackendSettings::kDeviceU);
  }
  else if (s.contains(BackendSettings::kDevice)) {
    device_ = s.value(BackendSettings::kDevice);
  }

  playbin3_enabled_ = s.value(BackendSettings::kPlaybin3, true).toBool();

  exclusive_mode_ = s.value(BackendSettings::kExclusiveMode, false).toBool();

  volume_control_ = s.value(BackendSettings::kVolumeControl, true).toBool();

  channels_enabled_ = s.value(BackendSettings::kChannelsEnabled, false).toBool();
  channels_ = s.value(BackendSettings::kChannels, 0).toInt();

  buffer_duration_nanosec_ = s.value(BackendSettings::kBufferDuration, BackendSettings::kDefaultBufferDuration).toULongLong() * kNsecPerMsec;
  buffer_low_watermark_ = s.value(BackendSettings::kBufferLowWatermark, BackendSettings::kDefaultBufferLowWatermark).toDouble();
  buffer_high_watermark_ = s.value(BackendSettings::kBufferHighWatermark, BackendSettings::kDefaultBufferHighWatermark).toDouble();

  rg_enabled_ = s.value(BackendSettings::kRgEnabled, false).toBool();
  rg_mode_ = s.value(BackendSettings::kRgMode, 0).toInt();
  rg_preamp_ = s.value(BackendSettings::kRgPreamp, 0.0).toDouble();
  rg_fallbackgain_ = s.value(BackendSettings::kRgFallbackGain, 0.0).toDouble();
  rg_compression_ = s.value(BackendSettings::kRgCompression, true).toBool();

  ebur128_loudness_normalization_ = s.value(BackendSettings::kEBUR128LoudnessNormalization, false).toBool();
  ebur128_target_level_lufs_ = s.value(BackendSettings::kEBUR128TargetLevelLUFS, -23.0).toDouble();

  fadeout_enabled_ = s.value(BackendSettings::kFadeoutEnabled, false).toBool();
  crossfade_enabled_ = s.value(BackendSettings::kCrossfadeEnabled, false).toBool();
  autocrossfade_enabled_ = s.value(BackendSettings::kAutoCrossfadeEnabled, false).toBool();
  crossfade_same_album_ = !s.value(BackendSettings::kNoCrossfadeSameAlbum, true).toBool();
  fadeout_pause_enabled_ = s.value(BackendSettings::kFadeoutPauseEnabled, false).toBool();
  fadeout_duration_ = s.value(BackendSettings::kFadeoutDuration, 2000).toLongLong();
  fadeout_duration_nanosec_ = (fadeout_duration_ * kNsecPerMsec);
  fadeout_pause_duration_ = s.value(BackendSettings::kFadeoutPauseDuration, 250).toLongLong();
  fadeout_pause_duration_nanosec_ = (fadeout_pause_duration_ * kNsecPerMsec);

  bs2b_enabled_ = s.value(BackendSettings::kBS2B, false).toBool();

  bool http2_enabled = s.value(BackendSettings::kHTTP2, false).toBool();
  if (http2_enabled != http2_enabled_) {
    http2_enabled_ = http2_enabled;
    Utilities::SetEnv("SOUP_FORCE_HTTP1", http2_enabled_ ? ""_L1 : u"1"_s);
    qLog(Debug) << "SOUP_FORCE_HTTP1:" << (http2_enabled_ ? "OFF" : "ON");
  }

  strict_ssl_enabled_ = s.value(BackendSettings::kStrictSSL, false).toBool();

  s.endGroup();

  s.beginGroup(NetworkProxySettings::kSettingsGroup);
  const NetworkProxyFactory::Mode proxy_mode = static_cast<NetworkProxyFactory::Mode>(s.value("mode", static_cast<int>(NetworkProxyFactory::Mode::System)).toInt());
  if (proxy_mode == NetworkProxyFactory::Mode::Manual && s.contains(NetworkProxySettings::kEngine) && s.value(NetworkProxySettings::kEngine).toBool()) {
    QString proxy_host = s.value(NetworkProxySettings::kHostname).toString();
    int proxy_port = s.value(NetworkProxySettings::kPort).toInt();
    if (proxy_host.isEmpty() || proxy_port <= 0) {
      proxy_address_.clear();
      proxy_authentication_ = false;
      proxy_user_.clear();
      proxy_pass_.clear();
    }
    else {
      proxy_address_ = QStringLiteral("%1:%2").arg(proxy_host).arg(proxy_port);
      proxy_authentication_ = s.value(NetworkProxySettings::kUseAuthentication).toBool();
      proxy_user_ = s.value(NetworkProxySettings::kUsername).toString();
      proxy_pass_ = s.value(NetworkProxySettings::kPassword).toString();
    }
  }
  else {
    proxy_address_.clear();
    proxy_authentication_ = false;
    proxy_user_.clear();
    proxy_pass_.clear();
  }

  s.endGroup();

#ifdef HAVE_SPOTIFY
  s.beginGroup(SpotifySettings::kSettingsGroup);
  spotify_access_token_ = s.value(SpotifySettings::kAccessToken).toString();
  s.endGroup();
#endif

}

void EngineBase::EmitAboutToFinish() {

  if (about_to_end_emitted_) {
    return;
  }

  about_to_end_emitted_ = true;

  Q_EMIT TrackAboutToEnd();

}

bool EngineBase::ValidOutput(const QString &output) {

  Q_UNUSED(output);

  return (true);

}

void EngineBase::UpdateSpotifyAccessToken(const QString &spotify_access_token) {

#ifdef HAVE_SPOTIFY

  spotify_access_token_ = spotify_access_token;

  SetSpotifyAccessToken();

#else

  Q_UNUSED(spotify_access_token)

#endif  // HAVE_SPOTIFY

}
