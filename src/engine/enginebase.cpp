/*
 * Strawberry Music Player
 * This file was part of Amarok / Clementine
 * Copyright 2003 Mark Kretschmann
 * Copyright 2004 - 2005 Max Howell, <max.howell@methylblue.com>
 * Copyright 2010 David Sansome <me@davidsansome.com>
 * Copyright 2017 - 2018 Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/timeconstants.h"
#include "engine_fwd.h"
#include "enginebase.h"
#include "settings/backendsettingspage.h"

Engine::Base::Base()
    : volume_(100),
      beginning_nanosec_(0),
      end_nanosec_(0),
      scope_(kScopeSize),
      output_(""),
      device_(QVariant()),
      rg_enabled_(false),
      rg_mode_(0),
      rg_preamp_(0),
      rg_compression_(true),
      buffer_duration_nanosec_(4000),
      buffer_min_fill_(33),
      fadeout_enabled_(true),
      crossfade_enabled_(true),
      autocrossfade_enabled_(false),
      crossfade_same_album_(false),
      fadeout_pause_enabled_(false),
      fadeout_duration_(2),
      fadeout_duration_nanosec_(2 * kNsecPerSec),
      about_to_end_emitted_(false) {}

Engine::Base::~Base() {}

bool Engine::Base::Load(const QUrl &stream_url, const QUrl &original_url, TrackChangeFlags, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {

  Q_UNUSED(force_stop_at_end);

  stream_url_ = stream_url;
  original_url_ = original_url;
  beginning_nanosec_ = beginning_nanosec;
  end_nanosec_ = end_nanosec;

  about_to_end_emitted_ = false;
  return true;

}

bool Engine::Base::Play(const QUrl &stream_url, const QUrl &original_url, TrackChangeFlags flags, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {

  if (!Load(stream_url, original_url, flags, force_stop_at_end, beginning_nanosec, end_nanosec))
    return false;

  return Play(0);

}

void Engine::Base::SetVolume(uint value) {

  volume_ = value;
  SetVolumeSW(MakeVolumeLogarithmic(value));

}

uint Engine::Base::MakeVolumeLogarithmic(uint volume) {
  // We're using a logarithmic function to make the volume ramp more natural.
  return static_cast<uint>( 100 - 100.0 * std::log10( ( 100 - volume ) * 0.09 + 1.0 ) );
}

void Engine::Base::ReloadSettings() {

  QSettings s;

  s.beginGroup(BackendSettingsPage::kSettingsGroup);

  output_ = s.value("output").toString();
  device_ = s.value("device");

  volume_control_ = s.value("volume_control", true).toBool();

  buffer_duration_nanosec_ = s.value("bufferduration", 4000).toLongLong() * kNsecPerMsec;
  buffer_min_fill_ = s.value("bufferminfill", 33).toInt();

  rg_enabled_ = s.value("rgenabled", false).toBool();
  rg_mode_ = s.value("rgmode", 0).toInt();
  rg_preamp_ = s.value("rgpreamp", 0.0).toDouble();
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

  s.endGroup();

}

void Engine::Base::EmitAboutToEnd() {

  if (about_to_end_emitted_)
    return;

  about_to_end_emitted_ = true;
  emit TrackAboutToEnd();
}

bool Engine::Base::ValidOutput(const QString &output) {

  Q_UNUSED(output);

  return (true);

}
