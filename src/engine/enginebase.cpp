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
#include "engine_fwd.h"
#include "enginebase.h"
#include "settings/backendsettingspage.h"
#include "settings/networkproxysettingspage.h"

Engine::Base::Base(const EngineType type, QObject *parent)
    : QObject(parent),
      type_(type),
      volume_control_(true),
      volume_(100),
      beginning_nanosec_(0),
      end_nanosec_(0),
      scope_(kScopeSize),
      buffering_(false),
      equalizer_enabled_(false),
      rg_enabled_(false),
      rg_mode_(0),
      rg_preamp_(0.0),
      rg_fallbackgain_(0.0),
      rg_compression_(true),
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
      about_to_end_emitted_(false) {}

Engine::Base::~Base() = default;

bool Engine::Base::Load(const QUrl &stream_url, const QUrl &original_url, const TrackChangeFlags, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec) {

  Q_UNUSED(force_stop_at_end);

  stream_url_ = stream_url;
  original_url_ = original_url;
  beginning_nanosec_ = beginning_nanosec;
  end_nanosec_ = end_nanosec;

  about_to_end_emitted_ = false;

  return true;

}

bool Engine::Base::Play(const QUrl &stream_url, const QUrl &original_url, const TrackChangeFlags flags, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const quint64 offset_nanosec) {

  if (!Load(stream_url, original_url, flags, force_stop_at_end, beginning_nanosec, end_nanosec)) {
    return false;
  }

  return Play(offset_nanosec);

}

void Engine::Base::UpdateVolume(const uint volume) {

  volume_ = volume;
  emit VolumeChanged(volume);

}

void Engine::Base::SetVolume(const uint volume) {

  volume_ = volume;
  SetVolumeSW(volume);

}

void Engine::Base::ReloadSettings() {

  QSettings s;

  s.beginGroup(BackendSettingsPage::kSettingsGroup);

  output_ = s.value("output").toString();
  device_ = s.value("device");

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
    Utilities::SetEnv("SOUP_FORCE_HTTP1", http2_enabled_ ? "" : "1");
    qLog(Debug) << "SOUP_FORCE_HTTP1:" << (http2_enabled_ ? "OFF" : "ON");
  }

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
      proxy_address_ = QString("%1:%2").arg(proxy_host).arg(proxy_port);
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

}

void Engine::Base::EmitAboutToEnd() {

  if (about_to_end_emitted_) {
    return;
  }

  about_to_end_emitted_ = true;
  emit TrackAboutToEnd();
}

bool Engine::Base::ValidOutput(const QString &output) {

  Q_UNUSED(output);

  return (true);

}
