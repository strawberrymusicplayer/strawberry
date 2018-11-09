/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include "version.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <deezer/deezer-connect.h>
#include <deezer/deezer-player.h>
#include <deezer/deezer-object.h>
#include <deezer/deezer-track.h>

#include <QtGlobal>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>

#include "core/timeconstants.h"
#include "core/taskmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "engine_fwd.h"
#include "enginebase.h"
#include "enginetype.h"
#include "deezerengine.h"
#include "deezer/deezerservice.h"
#include "settings/deezersettingspage.h"

const char *DeezerEngine::kAppID = "303684";
const char *DeezerEngine::kProductID = "strawberry";
const char *DeezerEngine::kProductVersion = STRAWBERRY_VERSION_DISPLAY;

DeezerEngine::DeezerEngine(TaskManager *task_manager)
  : EngineBase(),
    state_(Engine::Empty),
    position_(0),
    stopping_(false) {

  type_ = Engine::Deezer;

}

DeezerEngine::~DeezerEngine() {

  if (player_) {
    dz_object_release((dz_object_handle) player_);
    player_ = nullptr;
  }

  if (connect_) {
    dz_object_release((dz_object_handle) connect_);
    connect_ = nullptr;
  }

}

bool DeezerEngine::Init() {

  qLog(Debug) << "Deezer native SDK Version:" << dz_connect_get_build_id() << QCoreApplication::applicationName().toUtf8();

  struct dz_connect_configuration config;
  memset(&config, 0, sizeof(struct dz_connect_configuration));

  config.app_id = kAppID;
  config.product_id = kProductID;
  config.product_build_id = kProductVersion;
  config.connect_event_cb = ConnectEventCallback;

  connect_ = dz_connect_new(&config);
  if (!connect_) {
    qLog(Error) << "Deezer: Failed to create connect.";
    return false;
  }

  qLog(Debug) << "Device ID:" << dz_connect_get_device_id(connect_);

  dz_error_t dzerr(DZ_ERROR_NO_ERROR);

  dzerr = dz_connect_debug_log_disable(connect_);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to disable debug log.";
    return false;
  }

  dzerr = dz_connect_activate(connect_, this);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to activate connect.";
    return false;
  }

  dz_connect_cache_path_set(connect_, nullptr, nullptr, QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().constData());

  player_ = dz_player_new(connect_);
  if (!player_) {
    qLog(Error) << "Deezer: Failed to create player.";
    return false;
  }

  dzerr = dz_player_activate(player_, this);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to activate player.";
    return false;
  }

  dzerr = dz_player_set_event_cb(player_, PlayerEventCallback);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set event callback.";
    return false;
  }

  dzerr = dz_player_set_metadata_cb(player_, PlayerMetaDataCallback);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set metadata callback.";
    return false;
  }

  dzerr = dz_player_set_render_progress_cb(player_, PlayerProgressCallback, 1000);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set progress callback.";
    return false;
  }

  dzerr = dz_player_set_crossfading_duration(player_, nullptr, nullptr, 3000);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set crossfade duration.";
    return false;
  }

  dzerr = dz_connect_offline_mode(connect_, nullptr, nullptr, false);
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set offline mode.";
    return false;
  }

  LoadAccessToken();
  ReloadSettings();

  return true;

}

void DeezerEngine::ReloadSettings() {

  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  QString quality = s.value("quality", "FLAC").toString();
  s.endGroup();
  dz_error_t dzerr;

  if (quality == "MP3_128")
    dzerr = dz_player_set_track_quality(player_, nullptr, nullptr, DZ_TRACK_QUALITY_STANDARD);
  else if (quality == "MP3_320")
    dzerr = dz_player_set_track_quality(player_, nullptr, nullptr, DZ_TRACK_QUALITY_HIGHQUALITY);
  else if (quality == "FLAC")
    dzerr = dz_player_set_track_quality(player_, nullptr, nullptr, DZ_TRACK_QUALITY_CDQUALITY);
  else if (quality == "DATA_EFFICIENT")
    dzerr = dz_player_set_track_quality(player_, nullptr, nullptr, DZ_TRACK_QUALITY_DATA_EFFICIENT);
  else
    dzerr = dz_player_set_track_quality(player_, nullptr, nullptr, DZ_TRACK_QUALITY_CDQUALITY);

  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set quality.";
  }

}

bool DeezerEngine::Initialised() const {

  if (connect_ && player_) return true;
  return false;

}

void DeezerEngine::LoadAccessToken() {

  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  if (!s.contains("access_token") || !s.contains("expiry_time")) return;
  access_token_ = s.value("access_token").toString();
  expiry_time_ = s.value("expiry_time").toDateTime();
  s.endGroup();

  dz_error_t dzerr = dz_connect_set_access_token(connect_, nullptr, nullptr, access_token_.toUtf8().constData());
  if (dzerr != DZ_ERROR_NO_ERROR) {
    qLog(Error) << "Deezer: Failed to set access token.";
  }

}

bool DeezerEngine::Load(const QUrl &media_url, const QUrl &original_url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {

  if (!Initialised()) return false;
  stopping_ = false;

  Engine::Base::Load(media_url, original_url, change, force_stop_at_end, beginning_nanosec, end_nanosec);
  dz_error_t dzerr = dz_player_load(player_, nullptr, nullptr, media_url.toString().toUtf8().constData());
  if (dzerr != DZ_ERROR_NO_ERROR) return false;

  return true;

}

bool DeezerEngine::Play(quint64 offset_nanosec) {

  if (!Initialised()) return false;
  stopping_ = false;

  dz_error_t dzerr(DZ_ERROR_NO_ERROR);
  if (state() == Engine::Paused) dzerr = dz_player_resume(player_, nullptr, nullptr);
  else dzerr = dz_player_play(player_, nullptr, nullptr, DZ_PLAYER_PLAY_CMD_START_TRACKLIST, DZ_INDEX_IN_QUEUELIST_CURRENT);
  if (dzerr != DZ_ERROR_NO_ERROR) return false;

  Seek(offset_nanosec);

  return true;

}

void DeezerEngine::Stop(bool stop_after) {

  if (!Initialised()) return;
  stopping_ = true;

  dz_error_t dzerr = dz_player_stop(player_, nullptr, nullptr);
  if (dzerr != DZ_ERROR_NO_ERROR) return;

}

void DeezerEngine::Pause() {

  if (!Initialised()) return;

  dz_error_t dzerr = dz_player_pause(player_, nullptr, nullptr);
  if (dzerr != DZ_ERROR_NO_ERROR) return;

  state_ = Engine::Paused;
  emit StateChanged(state_);

}

void DeezerEngine::Unpause() {

  if (!Initialised()) return;
  dz_error_t dzerr = dz_player_resume(player_, nullptr, nullptr);
  if (dzerr != DZ_ERROR_NO_ERROR) return;

}

void DeezerEngine::Seek(quint64 offset_nanosec) {

  if (!Initialised()) return;

  stopping_ = false;
  dz_useconds_t offset = (offset_nanosec / kNsecPerUsec);
  dz_error_t dzerr = dz_player_seek(player_, nullptr, nullptr, offset);
  if (dzerr != DZ_ERROR_NO_ERROR) return;

}

void DeezerEngine::SetVolumeSW(uint percent) {

  if (!Initialised()) return;

  dz_error_t dzerr = dz_player_set_output_volume(player_, nullptr, nullptr, percent);
  if (dzerr != DZ_ERROR_NO_ERROR) qLog(Error) << "Deezer: Failed to set volume.";

}

qint64 DeezerEngine::position_nanosec() const {

  if (state() == Engine::Empty) return 0;
  const qint64 result = (position_ * kNsecPerUsec);
  return qint64(qMax(0ll, result));

}

qint64 DeezerEngine::length_nanosec() const {

  if (state() == Engine::Empty) return 0;
  const qint64 result = (end_nanosec_ - beginning_nanosec_);
  return result;

}

EngineBase::OutputDetailsList DeezerEngine::GetOutputsList() const {
  OutputDetailsList ret;
  OutputDetails output;
  output.name = "default";
  output.description = "Default";
  output.iconname = "soundcard";
  ret << output;
  return ret;
}

bool DeezerEngine::ValidOutput(const QString &output) {
  return(true);
}

bool DeezerEngine::CustomDeviceSupport(const QString &output) {
  return false;
}

bool DeezerEngine::ALSADeviceSupport(const QString &output) {
  return false;
}

bool DeezerEngine::CanDecode(const QUrl &url) {
  if (url.scheme() == "dzmedia") return true;
  else return false;
}

void DeezerEngine::ConnectEventCallback(dz_connect_handle handle, dz_connect_event_handle event, void *delegate) {

  dz_connect_event_t type = dz_connect_event_get_type(event);
  //DeezerEngine *engine = reinterpret_cast<DeezerEngine*>(delegate);

  switch (type) {
    case DZ_CONNECT_EVENT_USER_OFFLINE_AVAILABLE:
      qLog(Debug) << "CONNECT_EVENT USER_OFFLINE_AVAILABLE";
      break;

    case DZ_CONNECT_EVENT_USER_ACCESS_TOKEN_OK: {
        const char* szAccessToken;
        szAccessToken = dz_connect_event_get_access_token(event);
        qLog(Debug) << "CONNECT_EVENT USER_ACCESS_TOKEN_OK Access_token :" << szAccessToken;
      }
      break;

    case DZ_CONNECT_EVENT_USER_ACCESS_TOKEN_FAILED:
      qLog(Debug) << "CONNECT_EVENT USER_ACCESS_TOKEN_FAILED";
      break;

    case DZ_CONNECT_EVENT_USER_LOGIN_OK:
      qLog(Debug) << "Deezer CONNECT_EVENT USER_LOGIN_OK";
      break;

    case DZ_CONNECT_EVENT_USER_NEW_OPTIONS:
      qLog(Debug) << "Deezer: CONNECT_EVENT USER_NEW_OPTIONS";
      break;

    case DZ_CONNECT_EVENT_USER_LOGIN_FAIL_NETWORK_ERROR:
      qLog(Debug) << "Deezer: CONNECT_EVENT USER_LOGIN_FAIL_NETWORK_ERROR";
      break;

    case DZ_CONNECT_EVENT_USER_LOGIN_FAIL_BAD_CREDENTIALS:
      qLog(Debug) << "Deezer: CONNECT_EVENT USER_LOGIN_FAIL_BAD_CREDENTIALS";
      break;

    case DZ_CONNECT_EVENT_USER_LOGIN_FAIL_USER_INFO:
      qLog(Debug) << "Deezer: CONNECT_EVENT USER_LOGIN_FAIL_USER_INFO";
      break;

    case DZ_CONNECT_EVENT_USER_LOGIN_FAIL_OFFLINE_MODE:
      qLog(Debug) << "Deezer: CONNECT_EVENT USER_LOGIN_FAIL_OFFLINE_MODE";
      break;

    case DZ_CONNECT_EVENT_ADVERTISEMENT_START:
      qLog(Debug) << "Deezer: CONNECT_EVENTADVERTISEMENT_START";
      break;

    case DZ_CONNECT_EVENT_ADVERTISEMENT_STOP:
      qLog(Debug) << "Deezer: CONNECT_EVENTADVERTISEMENT_STOP";
      break;

    case DZ_CONNECT_EVENT_UNKNOWN:
    default:
      qLog(Debug) << "Deezer: CONNECT_EVENTUNKNOWN or default (type =" << type;
      break;
  }

}


void DeezerEngine::PlayerEventCallback(dz_player_handle handle, dz_player_event_handle event, void *supervisor) {

  DeezerEngine *engine = reinterpret_cast<DeezerEngine*>(supervisor);
  dz_streaming_mode_t streaming_mode;
  dz_index_in_queuelist idx;
  dz_player_event_t type = dz_player_event_get_type(event);

  if (!dz_player_event_get_queuelist_context(event, &streaming_mode, &idx)) {
    streaming_mode = DZ_STREAMING_MODE_ONDEMAND;
    idx = DZ_INDEX_IN_QUEUELIST_INVALID;
  }

  switch (type) {

    case DZ_PLAYER_EVENT_LIMITATION_FORCED_PAUSE:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_LOADED:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_NO_RIGHT:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_NEED_NATURAL_NEXT:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_TRACK_NOT_AVAILABLE_OFFLINE:
      engine->state_ = Engine::Error;
      emit engine->StateChanged(engine->state_);
      emit engine->InvalidSongRequested(engine->media_url_);
      emit engine->Error("Track not available offline.");
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_TRACK_RIGHTS_AFTER_AUDIOADS:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_SKIP_NO_RIGHT:
      break;

    case DZ_PLAYER_EVENT_QUEUELIST_TRACK_SELECTED:
      break;

    case DZ_PLAYER_EVENT_MEDIASTREAM_DATA_READY:
      break;

    case DZ_PLAYER_EVENT_MEDIASTREAM_DATA_READY_AFTER_SEEK:
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_START_FAILURE:
      engine->state_ = Engine::Error;
      emit engine->StateChanged(engine->state_);
      emit engine->InvalidSongRequested(engine->media_url_);
      emit engine->Error("Track start failure.");
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_START:
      engine->state_ = Engine::Playing;
      engine->position_ = 0;
      emit engine->StateChanged(engine->state_);
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_END:
      engine->state_ = Engine::Idle;
      engine->position_ = 0;
      emit engine->TrackEnded();
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_PAUSED:
      engine->state_ = Engine::Paused;
      emit engine->StateChanged(engine->state_);
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_UNDERFLOW:
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_RESUMED:
      engine->state_ = Engine::Playing;
      emit engine->StateChanged(engine->state_);
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_SEEKING:
      break;

    case DZ_PLAYER_EVENT_RENDER_TRACK_REMOVED:
      if (!engine->stopping_) return;
      engine->state_ = Engine::Empty;
      engine->position_ = 0;
      emit engine->StateChanged(engine->state_);
      break;

    case DZ_PLAYER_EVENT_UNKNOWN:
    default:
      qLog(Error) << "Deezer: Unknown player event" << type;
      break;
  }

}

void DeezerEngine::PlayerProgressCallback(dz_player_handle handle, dz_useconds_t progress, void *userdata) {
  DeezerEngine *engine = reinterpret_cast<DeezerEngine*>(userdata);
  engine->position_ = progress;
}

void DeezerEngine::PlayerMetaDataCallback(dz_player_handle handle, dz_track_metadata_handle metadata, void *userdata) {

  const dz_media_track_detailed_infos_t *track_metadata = dz_track_metadata_get_format_header(metadata);
  DeezerEngine *engine = reinterpret_cast<DeezerEngine*>(userdata);
  Engine::SimpleMetaBundle bundle;

  switch (track_metadata->format) {
    case DZ_MEDIA_FORMAT_AUDIO_MPEG:
      bundle.filetype = Song::FileType_MPEG;
      break;
    case DZ_MEDIA_FORMAT_AUDIO_FLAC:
      bundle.filetype = Song::FileType_FLAC;
      break;
    case DZ_MEDIA_FORMAT_AUDIO_PCM:
      bundle.filetype = Song::FileType_PCM;
      break;
    default:
      return;
  }

  bundle.url = engine->original_url_;
  bundle.title = QString();
  bundle.artist = QString();
  bundle.comment = QString();
  bundle.album = QString();
  bundle.length = 0;
  bundle.year = 0;
  bundle.tracknr = 0;
  bundle.samplerate = track_metadata->audio.samples.sample_rate;
  bundle.bitdepth = 0;
  bundle.bitrate = track_metadata->average_bitrate / 1000;
  bundle.lyrics = QString();

  emit engine->MetaData(bundle);

}
