/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <boost/bind.hpp>

#include <QTimer>
#include <QtDebug>
#include <QMutexLocker>
#include <QTime>

#include "vlcengine.h"
#include "vlcscopedref.h"

#include "core/taskmanager.h"
#include "core/logging.h"

VLCEngine* VLCEngine::sInstance = NULL;

VLCEngine::VLCEngine(TaskManager *task_manager)
  : instance_(NULL),
    player_(NULL),
    scope_data_(4096),
    state_(Engine::Empty)
{
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
#if 1
  static const char * const args[] = {
    "-I", "dummy",        // Don't use any interface
    "--ignore-config",    // Don't use VLC's config
    "--extraintf=logger", // log anything
    "--verbose=2",        // be much more verbose then normal for debugging purpose

    // Our scope plugin
    "--audio-filter=clementine_scope",
    "--no-plugins-cache",

    // Try to stop audio stuttering
    "--file-caching=500", // msec
    "--http-caching=500",

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    "--aout=alsa",        // The default, pulseaudio, is buggy
#endif
  };
#endif

  // Create the VLC instance
  //libvlc_exception_init(&exception_);
  instance_ = libvlc_new(sizeof(args) / sizeof(args[0]), args);
  HandleErrors();

  // Create the media player
  player_ = libvlc_media_player_new(instance_);
  HandleErrors();

  // Add event handlers
  libvlc_event_manager_t* player_em = libvlc_media_player_event_manager(player_);
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

  sInstance = this;
}

VLCEngine::~VLCEngine() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_media_player_stop(player_);
  libvlc_media_player_release(player_);
  libvlc_release(instance_);
  HandleErrors();
}

void VLCEngine::AttachCallback(libvlc_event_manager_t* em, libvlc_event_type_t type, libvlc_callback_t callback) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_event_attach(em, type, callback, this);
  HandleErrors();
}

void VLCEngine::StateChangedCallback(const libvlc_event_t* e, void* data) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  VLCEngine* engine = reinterpret_cast<VLCEngine*>(data);

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

bool VLCEngine::Init() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  type_ = Engine::VLC;
    
  return true;
}

bool VLCEngine::CanDecode(const QUrl &url) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  // TODO
  return true;
}

bool VLCEngine::Load(const QUrl &url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  // Create the media object
  VlcScopedRef<libvlc_media_t> media(libvlc_media_new_location(instance_, url.toEncoded().constData()));
  //if (libvlc_exception_raised(&exception_))
    //return false;

  libvlc_media_player_set_media(player_, media);
  //if (libvlc_exception_raised(&exception_))
    //return false;

  return true;
}

bool VLCEngine::Play(quint64 offset_nanosec) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  libvlc_media_player_play(player_);
  //if (libvlc_exception_raised(&exception_))
    //return false;

  Seek(offset_nanosec);

  return true;
  

}

void VLCEngine::Stop(bool stop_after) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_media_player_stop(player_);
  HandleErrors();
}

void VLCEngine::Pause() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_media_player_pause(player_);
  HandleErrors();
}

void VLCEngine::Unpause() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_media_player_play(player_);
  HandleErrors();
}

uint VLCEngine::position() const {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  //bool is_playing = libvlc_media_player_is_playing(player_, const_cast<libvlc_exception_t*>(&exception_));
  bool is_playing = libvlc_media_player_is_playing(player_);
  HandleErrors();

  if (!is_playing)
    return 0;

  //float pos = libvlc_media_player_get_position(player_, const_cast<libvlc_exception_t*>(&exception_));
  float pos = libvlc_media_player_get_position(player_);
  HandleErrors();

  return pos * length();

}

uint VLCEngine::length() const {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  //bool is_playing = libvlc_media_player_is_playing(player_, const_cast<libvlc_exception_t*>(&exception_));
  bool is_playing = libvlc_media_player_is_playing(player_);
  HandleErrors();

  if (!is_playing)
    return 0;

  //libvlc_time_t len = libvlc_media_player_get_length(player_, const_cast<libvlc_exception_t*>(&exception_));
  libvlc_time_t len = libvlc_media_player_get_length(player_);
  HandleErrors();

  return len;

}

void VLCEngine::Seek(quint64 offset_nanosec) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  uint len = length();
  if (len == 0)
    return;

  float pos = float(offset_nanosec) / len;

  libvlc_media_player_set_position(player_, pos);
  HandleErrors();

}

void VLCEngine::SetVolumeSW(uint percent) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  libvlc_audio_set_volume(player_, percent);
  HandleErrors();
}

void VLCEngine::HandleErrors() const {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  //if (libvlc_exception_raised(&exception_)) {
    //qFatal("libvlc error: %s", libvlc_exception_get_message(&exception_));
  //}

}

void VLCEngine::SetScopeData(float* data, int size) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  if (!sInstance)
    return;

  QMutexLocker l(&sInstance->scope_mutex_);

  // This gets called by our VLC plugin.  Just push the data on to the end of
  // the circular buffer and let it get consumed by scope()
  for (int i=0 ; i<size ; ++i) {
    sInstance->scope_data_.push_back(data[i]);
  }
}

const Engine::Scope& VLCEngine::Scope() {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  QMutexLocker l(&scope_mutex_);

  // Leave the scope unchanged if there's not enough data
  if (scope_data_.size() < uint(kScopeSize))
    return scope_;

  // Take the samples off the front of the circular buffer
  for (uint i=0 ; i<uint(kScopeSize) ; ++i)
    scope_[i] = scope_data_[i] * (1 << 15);

  // Remove the samples from the buffer.  Unfortunately I think this is O(n) :(
  scope_data_.rresize(qMax(0, int(scope_data_.size()) - kScopeSize*2));

  return scope_;

}

qint64 VLCEngine::position_nanosec() const {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  return 0;

}

qint64 VLCEngine::length_nanosec() const {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  return 0;

}

