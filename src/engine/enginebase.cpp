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
#include "utilities/timeconstants.h"
#include "core/networkproxyfactory.h"
#include "core/settings.h"
#include "enginebase.h"
#include "settings/backendsettingspage.h"
#include "settings/networkproxysettingspage.h"
#ifdef HAVE_SPOTIFY
#  include "settings/spotifysettingspage.h"
#endif

using namespace Qt::StringLiterals;

EngineBase::EngineBase(QObject *parent)
    : QObject(parent),
      exclusive_mode_(false),
      volume_control_(true),
      volume_(100),
      beginning_nanosec_(0),
      end_nanosec_(0),
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
      buffer_duration_nanosec_(BackendSettingsPage::kDefaultBufferDuration * kNsecPerMsec),
      buffer_low_watermark_(BackendSettingsPage::kDefaultBufferLowWatermark),
      buffer_high_watermark_(BackendSettingsPage::kDefaultBufferHighWatermark),
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

EngineBase::Type EngineBase::TypeFromName(const QString &name) {

  if (name.compare("gstreamer"_L1, Qt::CaseInsensitive) == 0) return Type::GStreamer;
  if (name.compare("vlc"_L1, Qt::CaseInsensitive) == 0)  return Type::VLC;

  return Type::None;

}

QString EngineBase::Name(const Type type) {

  switch (type) {
    case Type::GStreamer:  return QStringLiteral("gstreamer");
    case Type::VLC:        return QStringLiteral("vlc");
    case Type::None:
    default:               return QStringLiteral("None");
  }

}

QString EngineBase::Description(const Type type) {

  switch (type) {
    case Type::GStreamer:  return QStringLiteral("GStreamer");
    case Type::VLC:        return QStringLiteral("VLC");
    case Type::None:
    default:               return QStringLiteral("None");
  }

}

bool EngineBase::Load(const QUrl &media_url, const QUrl &stream_url, const TrackChangeFlags, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  Q_UNUSED(force_stop_at_end);

  media_url_ = media_url;
  stream_url_ = stream_url;
  beginning_nanosec_ = beginning_nanosec;
  end_nanosec_ = end_nanosec;

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

bool EngineBase::Play(const QUrl &media_url, const QUrl &stream_url, const bool pause, const TrackChangeFlags flags, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const quint64 offset_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  if (!Load(media_url, stream_url, flags, force_stop_at_end, beginning_nanosec, end_nanosec, ebur128_integrated_loudness_lufs)) {
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

  s.beginGroup(BackendSettingsPage::kSettingsGroup);

  output_ = s.value("output").toString();
  device_ = s.value("device");

  exclusive_mode_ = s.value("exclusive_mode", false).toBool();

  volume_control_ = s.value("volume_control", true).toBool();

  channels_enabled_ = s.value("channels_enabled", false).toBool();
  channels_ = s.value("channels", 0).toInt();

  buffer_duration_nanosec_ = s.value("bufferduration", BackendSettingsPage::kDefaultBufferDuration).toLongLong() * kNsecPerMsec;
  buffer_low_watermark_ = s.value("bufferlowwatermark", BackendSettingsPage::kDefaultBufferLowWatermark).toDouble();
  buffer_high_watermark_ = s.value("bufferhighwatermark", BackendSettingsPage::kDefaultBufferHighWatermark).toDouble();

  rg_enabled_ = s.value("rgenabled", false).toBool();
  rg_mode_ = s.value("rgmode", 0).toInt();
  rg_preamp_ = s.value("rgpreamp", 0.0).toDouble();
  rg_fallbackgain_ = s.value("rgfallbackgain", 0.0).toDouble();
  rg_compression_ = s.value("rgcompression", true).toBool();

  ebur128_loudness_normalization_ = s.value("ebur128_loudness_normalization", false).toBool();
  ebur128_target_level_lufs_ = s.value("ebur128_target_level_lufs", -23.0).toDouble();

  fadeout_enabled_ = s.value("FadeoutEnabled", false).toBool();
  crossfade_enabled_ = s.value("CrossfadeEnabled", false).toBool();
  autocrossfade_enabled_ = s.value("AutoCrossfadeEnabled", false).toBool();
  crossfade_same_album_ = !s.value("NoCrossfadeSameAlbum", true).toBool();
  fadeout_pause_enabled_ = s.value("FadeoutPauseEnabled", false).toBool();
  fadeout_duration_ = s.value("FadeoutDuration", 2000).toLongLong();
  fadeout_duration_nanosec_ = (fadeout_duration_ * kNsecPerMsec);
  fadeout_pause_duration_ = s.value("FadeoutPauseDuration", 250).toLongLong();
  fadeout_pause_duration_nanosec_ = (fadeout_pause_duration_ * kNsecPerMsec);

  bs2b_enabled_ = s.value("bs2b", false).toBool();

  bool http2_enabled = s.value("http2", false).toBool();
  if (http2_enabled != http2_enabled_) {
    http2_enabled_ = http2_enabled;
    Utilities::SetEnv("SOUP_FORCE_HTTP1", http2_enabled_ ? ""_L1 : QStringLiteral("1"));
    qLog(Debug) << "SOUP_FORCE_HTTP1:" << (http2_enabled_ ? "OFF" : "ON");
  }

  strict_ssl_enabled_ = s.value("strict_ssl", false).toBool();

  s.endGroup();

  s.beginGroup(NetworkProxySettingsPage::kSettingsGroup);
  const NetworkProxyFactory::Mode proxy_mode = static_cast<NetworkProxyFactory::Mode>(s.value("mode", static_cast<int>(NetworkProxyFactory::Mode::System)).toInt());
  if (proxy_mode == NetworkProxyFactory::Mode::Manual && s.contains("engine") && s.value("engine").toBool()) {
    QString proxy_host = s.value("hostname").toString();
    int proxy_port = s.value("port").toInt();
    if (proxy_host.isEmpty() || proxy_port <= 0) {
      proxy_address_.clear();
      proxy_authentication_ = false;
      proxy_user_.clear();
      proxy_pass_.clear();
    }
    else {
      proxy_address_ = QStringLiteral("%1:%2").arg(proxy_host).arg(proxy_port);
      proxy_authentication_ = s.value("use_authentication").toBool();
      proxy_user_ = s.value("username").toString();
      proxy_pass_ = s.value("password").toString();
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
  s.beginGroup(SpotifySettingsPage::kSettingsGroup);
  spotify_username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) spotify_password_.clear();
  else spotify_password_ = QString::fromUtf8(QByteArray::fromBase64(password));
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
