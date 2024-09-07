/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2017-2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <optional>

#include <vlc/vlc.h>

#include <QtGlobal>
#include <QMetaType>
#include <QVariant>
#include <QByteArray>
#include <QUrl>

#include "core/shared_ptr.h"
#include "core/taskmanager.h"
#include "core/logging.h"
#include "utilities/timeconstants.h"
#include "enginebase.h"
#include "vlcengine.h"
#include "vlcscopedref.h"

using namespace Qt::StringLiterals;

VLCEngine::VLCEngine(SharedPtr<TaskManager> task_manager, QObject *parent)
    : EngineBase(parent),
      instance_(nullptr),
      player_(nullptr),
      state_(State::Empty) {

  Q_UNUSED(task_manager);

  ReloadSettings();

}

VLCEngine::~VLCEngine() {

  if (state_ == State::Playing || state_ == State::Paused) {
    libvlc_media_player_stop(player_);
  }

  libvlc_event_manager_t *player_em = libvlc_media_player_event_manager(player_);
  if (player_em) {
    libvlc_event_detach(player_em, libvlc_MediaPlayerEncounteredError, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerNothingSpecial, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerOpening, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerBuffering, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerPlaying, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerPaused, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerStopped, StateChangedCallback, this);
    libvlc_event_detach(player_em, libvlc_MediaPlayerEndReached, StateChangedCallback, this);
  }

  libvlc_media_player_release(player_);
  libvlc_release(instance_);

}

bool VLCEngine::Init() {

  // Create the VLC instance
  instance_ = libvlc_new(0, nullptr);
  if (!instance_) return false;

  // Create the media player
  player_ = libvlc_media_player_new(instance_);
  if (!player_) return false;

  // Add event handlers
  libvlc_event_manager_t *player_em = libvlc_media_player_event_manager(player_);
  if (!player_em) return false;

  AttachCallback(player_em, libvlc_MediaPlayerEncounteredError, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerNothingSpecial, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerOpening, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerBuffering, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerPlaying, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerPaused, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerStopped, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerEndReached, StateChangedCallback);

  return true;

}

bool VLCEngine::Load(const QUrl &media_url, const QUrl &stream_url, const EngineBase::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  // FIXME: why is this not calling `EngineBase::Load()`?

  Q_UNUSED(media_url);
  Q_UNUSED(ebur128_integrated_loudness_lufs);
  Q_UNUSED(change);
  Q_UNUSED(force_stop_at_end);
  Q_UNUSED(beginning_nanosec);
  Q_UNUSED(end_nanosec);

  if (!Initialized()) return false;

  // Create the media object
  VlcScopedRef<libvlc_media_t> media(libvlc_media_new_location(instance_, stream_url.toEncoded().constData()));

  libvlc_media_player_set_media(player_, media);

  return true;

}

bool VLCEngine::Play(const bool pause, const quint64 offset_nanosec) {

  Q_UNUSED(pause);

  if (!Initialized()) return false;

  // Set audio output
  if (!output_.isEmpty() && output_ != "auto"_L1) {
    int result = libvlc_audio_output_set(player_, output_.toUtf8().constData());
    if (result != 0) qLog(Error) << "Failed to set output to" << output_;
  }

  // Set audio device
  if (device_.isValid() && device_.metaType().id() == QMetaType::QString && !device_.toString().isEmpty()) {
    libvlc_audio_output_device_set(player_, nullptr, device_.toString().toLocal8Bit().data());
  }

  int result = libvlc_media_player_play(player_);
  if (result != 0) return false;

  Seek(offset_nanosec);

  return true;

}

void VLCEngine::Stop(const bool stop_after) {

  Q_UNUSED(stop_after);

  if (!Initialized()) return;
  libvlc_media_player_stop(player_);

}

void VLCEngine::Pause() {

  if (!Initialized()) return;
  libvlc_media_player_pause(player_);

}

void VLCEngine::Unpause() {

  if (!Initialized()) return;
  libvlc_media_player_play(player_);

}

void VLCEngine::Seek(const quint64 offset_nanosec) {

  if (!Initialized()) return;

  int offset = static_cast<int>(offset_nanosec / kNsecPerMsec);

  uint len = length();
  if (len == 0) return;

  float pos = static_cast<float>(offset) / static_cast<float>(len);

  libvlc_media_player_set_position(player_, pos);

}

void VLCEngine::SetVolumeSW(const uint percent) {

  if (!Initialized()) return;
  if (!volume_control_ && percent != 100) return;
  libvlc_audio_set_volume(player_, static_cast<int>(percent));

}

qint64 VLCEngine::position_nanosec() const {

  if (state_ == State::Empty) return 0;
  const qint64 result = (position() * kNsecPerMsec);
  return qMax(0LL, result);

}

qint64 VLCEngine::length_nanosec() const {

  if (state_ == State::Empty) return 0;
  const qint64 result = (end_nanosec_ - static_cast<qint64>(beginning_nanosec_));
  if (result > 0) {
    return result;
  }

  // Get the length from the pipeline if we don't know.
  return (length() * kNsecPerMsec);

}

EngineBase::OutputDetailsList VLCEngine::GetOutputsList() const {

  OutputDetailsList outputs;
  OutputDetails output_auto;
  output_auto.name = "auto"_L1;
  output_auto.description = "Automatically detected"_L1;
  outputs << output_auto;

  libvlc_audio_output_t *audio_output_list = libvlc_audio_output_list_get(instance_);
  for (libvlc_audio_output_t *audio_output = audio_output_list; audio_output; audio_output = audio_output->p_next) {
    OutputDetails output;
    output.name = QString::fromUtf8(audio_output->psz_name);
    output.description = QString::fromUtf8(audio_output->psz_description);
    if (output.name == "auto"_L1) output.iconname = "soundcard"_L1;
    else if ((output.name == "alsa"_L1)||(output.name == "oss"_L1)) output.iconname = "alsa"_L1;
    else if (output.name== "jack"_L1) output.iconname = "jack"_L1;
    else if (output.name == "pulse"_L1) output.iconname = "pulseaudio"_L1;
    else if (output.name == "afile"_L1) output.iconname = "document-new"_L1;
    else output.iconname = "soundcard"_L1;
    outputs << output;
  }
  libvlc_audio_output_list_release(audio_output_list);

  return outputs;

}

bool VLCEngine::ValidOutput(const QString &output) {

  const OutputDetailsList output_details = GetOutputsList();
  return std::any_of(output_details.begin(), output_details.end(), [output](const OutputDetails &output_detail) { return output_detail.name == output; });

}

bool VLCEngine::CustomDeviceSupport(const QString &output) const {
  return output != "auto"_L1;
}

bool VLCEngine::ALSADeviceSupport(const QString &output) const {
  return output == "alsa"_L1;
}

bool VLCEngine::ExclusiveModeSupport(const QString &output) const {
  Q_UNUSED(output);
  return false;
}

uint VLCEngine::position() const {

  if (!Initialized() || !libvlc_media_player_is_playing(player_)) return 0;

  float pos = libvlc_media_player_get_position(player_);
  return static_cast<uint>(pos * static_cast<float>(length()));

}

uint VLCEngine::length() const {

  if (!Initialized() || !libvlc_media_player_is_playing(player_)) return 0;

  return libvlc_media_player_get_length(player_);

}

bool VLCEngine::CanDecode(const QUrl &url) { Q_UNUSED(url); return true; }

void VLCEngine::AttachCallback(libvlc_event_manager_t *em, libvlc_event_type_t type, libvlc_callback_t callback) {

  if (libvlc_event_attach(em, type, callback, this) != 0) {
    qLog(Error) << "Failed to attach callback.";
  }

}

void VLCEngine::StateChangedCallback(const libvlc_event_t *e, void *data) {

  VLCEngine *engine = reinterpret_cast<VLCEngine*>(data);

  switch (e->type) {
    case libvlc_MediaPlayerNothingSpecial:
    case libvlc_MediaPlayerOpening:
    case libvlc_MediaPlayerBuffering:
      break;

    case libvlc_MediaPlayerStopped:{
      const EngineBase::State state = engine->state_;
      engine->state_ = EngineBase::State::Empty;
      if (state == EngineBase::State::Playing) {
        Q_EMIT engine->StateChanged(engine->state_);
      }
      break;
    }

    case libvlc_MediaPlayerEncounteredError:
      engine->state_ = EngineBase::State::Error;
      Q_EMIT engine->StateChanged(engine->state_);
      Q_EMIT engine->FatalError();
      break;

    case libvlc_MediaPlayerPlaying:
      engine->state_ = EngineBase::State::Playing;
      Q_EMIT engine->StateChanged(engine->state_);
      break;

    case libvlc_MediaPlayerPaused:
      engine->state_ = EngineBase::State::Paused;
      Q_EMIT engine->StateChanged(engine->state_);
      break;

    case libvlc_MediaPlayerEndReached:
      engine->state_ = EngineBase::State::Idle;
      Q_EMIT engine->TrackEnded();
      break;
  }

}

void VLCEngine::GetDevicesList(const QString &output) const {

  Q_UNUSED(output);

  libvlc_audio_output_device_t *audio_output_device_list = libvlc_audio_output_device_list_get(instance_, output_.toUtf8().constData());
  for (libvlc_audio_output_device_t *audio_device = audio_output_device_list; audio_device; audio_device = audio_device->p_next) {
    qLog(Debug) << audio_device->psz_device << audio_device->psz_description;
  }
  libvlc_audio_output_device_list_release(audio_output_device_list);

}
