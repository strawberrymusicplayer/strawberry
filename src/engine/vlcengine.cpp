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

#include <stddef.h>
#include <vlc/vlc.h>

#include <QtGlobal>
#include <QByteArray>
#include <QUrl>

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

  type_ = Engine::VLC;
  ReloadSettings();

}

VLCEngine::~VLCEngine() {
    
  libvlc_media_player_stop(player_);
  libvlc_media_player_release(player_);
  libvlc_release(instance_);
  HandleErrors();

}

bool VLCEngine::Init() {

  const char *args[] = {
    //"--verbose=3",
    "--ignore-config",
    "--no-plugins-cache",
    "--no-xlib",
    "--no-video",
  };

  // Create the VLC instance
  instance_ = libvlc_new(sizeof(args) / sizeof(*args), args);
  HandleErrors();

  // Create the media player
  player_ = libvlc_media_player_new(instance_);
  HandleErrors();

  // Add event handlers
  libvlc_event_manager_t *player_em = libvlc_media_player_event_manager(player_);
  HandleErrors();

  AttachCallback(player_em, libvlc_MediaPlayerEncounteredError, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerNothingSpecial, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerOpening, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerBuffering, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerPlaying, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerPaused, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerStopped, StateChangedCallback);
  AttachCallback(player_em, libvlc_MediaPlayerEndReached, StateChangedCallback);
  HandleErrors();

  return true;

}

bool VLCEngine::Initialised() const {

  if (instance_ && player_) return true;
  return false;

}

bool VLCEngine::Load(const QUrl &url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {

  if (!Initialised()) return false;

  // Create the media object
  VlcScopedRef<libvlc_media_t> media(libvlc_media_new_location(instance_, url.toEncoded().constData()));

  libvlc_media_player_set_media(player_, media);

  return true;

}

bool VLCEngine::Play(quint64 offset_nanosec) {

  if (!Initialised()) return false;

  // Set audio output
  if (!output_.isEmpty() || output_ != "auto") {
    int result = libvlc_audio_output_set(player_, output_.toUtf8().constData());
    if (result != 0) qLog(Error) << "Failed to set output.";
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

void VLCEngine::Stop(bool stop_after) {

  if (!Initialised()) return;
  libvlc_media_player_stop(player_);
  HandleErrors();

}

void VLCEngine::Pause() {

  if (!Initialised()) return;
  libvlc_media_player_pause(player_);
  HandleErrors();

}

void VLCEngine::Unpause() {

  if (!Initialised()) return;
  libvlc_media_player_play(player_);
  HandleErrors();

}

void VLCEngine::Seek(quint64 offset_nanosec) {

  if (!Initialised()) return;

  int offset = (offset_nanosec / kNsecPerMsec);

  uint len = length();
  if (len == 0) return;

  float pos = float(offset) / len;

  libvlc_media_player_set_position(player_, pos);
  HandleErrors();

}

void VLCEngine::SetVolumeSW(uint percent) {
  if (!Initialised()) return;
  libvlc_audio_set_volume(player_, percent);
  HandleErrors();
}

qint64 VLCEngine::position_nanosec() const {
  if (state() == Engine::Empty) return 0;
  const qint64 result = (position() * kNsecPerMsec);
  return qint64(qMax(0ll, result));

}

qint64 VLCEngine::length_nanosec() const {
  if (state() == Engine::Empty) return 0;
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

uint VLCEngine::position() const {

  if (!Initialised()) return (0);

  bool is_playing = libvlc_media_player_is_playing(player_);
  HandleErrors();

  if (!is_playing) return 0;

  float pos = libvlc_media_player_get_position(player_);
  HandleErrors();

  return (pos * length());

}

uint VLCEngine::length() const {

  if (!Initialised()) return(0);

  bool is_playing = libvlc_media_player_is_playing(player_);
  HandleErrors();

  if (!is_playing) return 0;

  libvlc_time_t len = libvlc_media_player_get_length(player_);
  HandleErrors();

  return len;

}

bool VLCEngine::CanDecode(const QUrl &url) {
    
  // TODO
  return true;
}

void VLCEngine::HandleErrors() const {
}

void VLCEngine::AttachCallback(libvlc_event_manager_t *em, libvlc_event_type_t type, libvlc_callback_t callback) {

  libvlc_event_attach(em, type, callback, this);
  HandleErrors();

}

void VLCEngine::StateChangedCallback(const libvlc_event_t *e, void *data) {
    
  VLCEngine *engine = reinterpret_cast<VLCEngine*>(data);

  switch (e->type) {
    case libvlc_MediaPlayerNothingSpecial:
    case libvlc_MediaPlayerStopped:
    case libvlc_MediaPlayerEncounteredError:
      engine->state_ = Engine::Empty;
      break;

    case libvlc_MediaPlayerOpening:
    case libvlc_MediaPlayerBuffering:
    case libvlc_MediaPlayerPlaying:
      engine->state_ = Engine::Playing;
      break;

    case libvlc_MediaPlayerPaused:
      engine->state_ = Engine::Paused;
      break;

    case libvlc_MediaPlayerEndReached:
      engine->state_ = Engine::Idle;
      emit engine->TrackEnded();
      return; // Don't emit state changed here
  }

  emit engine->StateChanged(engine->state_);

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

void VLCEngine::GetDevicesList(QString output) const {

  libvlc_audio_output_device_t *audio_output_device_list = libvlc_audio_output_device_list_get(instance_, output_.toUtf8().constData());
  for (libvlc_audio_output_device_t *audio_device = audio_output_device_list ; audio_device ; audio_device = audio_device->p_next) {
    qLog(Debug) << audio_device->psz_device << audio_device->psz_description;
  }
  libvlc_audio_output_device_list_release(audio_output_device_list);

}
