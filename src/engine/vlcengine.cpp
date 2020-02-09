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

#include <vlc/vlc.h>

#include <QtGlobal>
#include <QVariant>
#include <QByteArray>
#include <QUrl>
#include <QtDebug>

#include "core/timeconstants.h"
#include "core/taskmanager.h"
#include "core/logging.h"
#include "engine_fwd.h"
#include "enginebase.h"
#include "enginetype.h"
#include "vlcengine.h"
#include "vlcscopedref.h"

VLCEngine::VLCEngine(TaskManager *task_manager)
  : EngineBase(),
    instance_(nullptr),
    player_(nullptr),
    state_(Engine::Empty) {

  Q_UNUSED(task_manager);

  type_ = Engine::VLC;
  ReloadSettings();

}

VLCEngine::~VLCEngine() {

  if (state_ == Engine::Playing || state_ == Engine::Paused) {
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

bool VLCEngine::Load(const QUrl &stream_url, const QUrl &original_url, const Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec) {

  Q_UNUSED(original_url);
  Q_UNUSED(change);
  Q_UNUSED(force_stop_at_end);
  Q_UNUSED(beginning_nanosec);
  Q_UNUSED(end_nanosec);

  if (!Initialised()) return false;

  // Create the media object
  VlcScopedRef<libvlc_media_t> media(libvlc_media_new_location(instance_, stream_url.toEncoded().constData()));

  libvlc_media_player_set_media(player_, media);

  return true;

}

bool VLCEngine::Play(const quint64 offset_nanosec) {

  if (!Initialised()) return false;

  // Set audio output
  if (!output_.isEmpty() && output_ != "auto") {
    int result = libvlc_audio_output_set(player_, output_.toUtf8().constData());
    if (result != 0) qLog(Error) << "Failed to set output to" << output_;
  }

  // Set audio device
  if (device_.isValid() && device_.type() == QVariant::String && !device_.toString().isEmpty()) {
    libvlc_audio_output_device_set(player_, nullptr, device_.toString().toLocal8Bit().data());
  }

  int result = libvlc_media_player_play(player_);
  if (result != 0) return false;

  Seek(offset_nanosec);

  return true;

}

void VLCEngine::Stop(const bool stop_after) {

  Q_UNUSED(stop_after);

  if (!Initialised()) return;
  libvlc_media_player_stop(player_);

}

void VLCEngine::Pause() {

  if (!Initialised()) return;
  libvlc_media_player_pause(player_);

}

void VLCEngine::Unpause() {

  if (!Initialised()) return;
  libvlc_media_player_play(player_);

}

void VLCEngine::Seek(const quint64 offset_nanosec) {

  if (!Initialised()) return;

  int offset = (offset_nanosec / kNsecPerMsec);

  uint len = length();
  if (len == 0) return;

  float pos = float(offset) / len;

  libvlc_media_player_set_position(player_, pos);

}

void VLCEngine::SetVolumeSW(const uint percent) {
  if (!Initialised()) return;
  if (!volume_control_ && percent != 100) return;
  libvlc_audio_set_volume(player_, percent);
}

qint64 VLCEngine::position_nanosec() const {
  if (state_ == Engine::Empty) return 0;
  const qint64 result = (position() * kNsecPerMsec);
  return qint64(qMax(0ll, result));

}

qint64 VLCEngine::length_nanosec() const {
  if (state_ == Engine::Empty) return 0;
  const qint64 result = (end_nanosec_ - beginning_nanosec_);
  if (result > 0) {
    return result;
  }
  else {
    // Get the length from the pipeline if we don't know.
    return (length() * kNsecPerMsec);
  }
}

EngineBase::OutputDetailsList VLCEngine::GetOutputsList() const {

  OutputDetailsList ret;

  PluginDetailsList plugins = GetPluginList();
  for (const PluginDetails &plugin : plugins) {
    OutputDetails output;
    output.name = plugin.name;
    output.description = plugin.description;
    if (plugin.name == "auto") output.iconname = "soundcard";
    else if ((plugin.name == "alsa")||(plugin.name == "oss")) output.iconname = "alsa";
    else if (plugin.name== "jack") output.iconname = "jack";
    else if (plugin.name == "pulse") output.iconname = "pulseaudio";
    else if (plugin.name == "afile") output.iconname = "document-new";
    else output.iconname = "soundcard";
    ret.append(output);
  }

  return ret;

}

bool VLCEngine::ValidOutput(const QString &output) {

  PluginDetailsList plugins = GetPluginList();
  for (const PluginDetails &plugin : plugins) {
    if (plugin.name == output) return(true);
  }
  return(false);

}

bool VLCEngine::CustomDeviceSupport(const QString &output) {
  return (output == "auto" ? false : true);
}

bool VLCEngine::ALSADeviceSupport(const QString &output) {
  return (output == "alsa");
}

uint VLCEngine::position() const {

  if (!Initialised()) return (0);

  bool is_playing = libvlc_media_player_is_playing(player_);
  if (!is_playing) return 0;

  float pos = libvlc_media_player_get_position(player_);
  return (pos * length());

}

uint VLCEngine::length() const {

  if (!Initialised()) return(0);

  bool is_playing = libvlc_media_player_is_playing(player_);
  if (!is_playing) return 0;

  libvlc_time_t len = libvlc_media_player_get_length(player_);

  return len;

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
      break;

    case libvlc_MediaPlayerStopped:
      engine->state_ = Engine::Empty;
      emit engine->StateChanged(engine->state_);
      break;

    case libvlc_MediaPlayerEncounteredError:
      engine->state_ = Engine::Error;
      emit engine->StateChanged(engine->state_);
      emit engine->FatalError();
      break;

    case libvlc_MediaPlayerOpening:
    case libvlc_MediaPlayerBuffering:
    case libvlc_MediaPlayerPlaying:
      engine->state_ = Engine::Playing;
      emit engine->StateChanged(engine->state_);
      break;

    case libvlc_MediaPlayerPaused:
      engine->state_ = Engine::Paused;
      emit engine->StateChanged(engine->state_);
      break;

    case libvlc_MediaPlayerEndReached:
      engine->state_ = Engine::Idle;
      emit engine->TrackEnded();
      return; // Don't emit state changed here
  }

}

EngineBase::PluginDetailsList VLCEngine::GetPluginList() const {

  PluginDetailsList ret;
  libvlc_audio_output_t *audio_output_list = libvlc_audio_output_list_get(instance_);

  {
    PluginDetails details;
    details.name = "auto";
    details.description = "Automatically detected";
    ret << details;
  }

  for (libvlc_audio_output_t *audio_output = audio_output_list ; audio_output ; audio_output = audio_output->p_next) {
    PluginDetails details;
    details.name = QString::fromUtf8(audio_output->psz_name);
    details.description = QString::fromUtf8(audio_output->psz_description);
    ret << details;
    //GetDevicesList(audio_output->psz_name);
  }

  libvlc_audio_output_list_release(audio_output_list);

  return ret;

}

void VLCEngine::GetDevicesList(const QString &output) const {

  Q_UNUSED(output);

  libvlc_audio_output_device_t *audio_output_device_list = libvlc_audio_output_device_list_get(instance_, output_.toUtf8().constData());
  for (libvlc_audio_output_device_t *audio_device = audio_output_device_list ; audio_device ; audio_device = audio_device->p_next) {
    qLog(Debug) << audio_device->psz_device << audio_device->psz_description;
  }
  libvlc_audio_output_device_list_release(audio_output_device_list);

}
