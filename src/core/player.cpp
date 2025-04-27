/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "player.h"

#include <algorithm>
#include <memory>
#include <chrono>

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QTimer>
#include <QSettings>

#include "constants/behavioursettings.h"
#include "constants/playlistsettings.h"
#include "constants/timeconstants.h"

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/urlhandlers.h"
#include "core/urlhandler.h"
#include "core/enginemetadata.h"

#include "engine/enginebase.h"
#include "engine/gstengine.h"

#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlist/playlistfilter.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "equalizer/equalizer.h"
#include "analyzer/analyzercontainer.h"

using namespace std::chrono_literals;
using std::make_shared;

namespace {
constexpr char kSettingsGroup[] = "Player";
constexpr char kVolume[] = "volume";
constexpr char kPlaybackState[] = "playback_state";
constexpr char kPlaybackPlaylist[] = "playback_playlist";
constexpr char kPlaybackPosition[] = "playback_position";
}  // namespace

Player::Player(const SharedPtr<TaskManager> task_manager, const SharedPtr<UrlHandlers> url_handlers, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : PlayerInterface(parent),
      task_manager_(task_manager),
      url_handlers_(url_handlers),
      playlist_manager_(playlist_manager),
      engine_(make_shared<GstEngine>(task_manager_)),
      analyzer_(nullptr),
      equalizer_(nullptr),
      timer_save_volume_(new QTimer(this)),
      playlists_loaded_(false),
      play_requested_(false),
      pause_(false),
      stream_change_type_(EngineBase::TrackChangeType::First),
      autoscroll_(Playlist::AutoScroll::Maybe),
      last_state_(EngineBase::State::Empty),
      nb_errors_received_(0),
      volume_(100),
      volume_before_mute_(100),
      last_pressed_previous_(QDateTime::currentDateTime()),
      continue_on_error_(false),
      greyout_(true),
      menu_previousmode_(BehaviourSettings::PreviousBehaviour::DontRestart),
      seek_step_sec_(10),
      volume_increment_(5),
      play_offset_nanosec_(0) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  timer_save_volume_->setSingleShot(true);
  timer_save_volume_->setInterval(5s);
  QObject::connect(timer_save_volume_, &QTimer::timeout, this, &Player::SaveVolume);

  QObject::connect(&*url_handlers, &UrlHandlers::Registered, this, &Player::UrlHandlerRegistered);

}

void Player::Init() {

  if (!engine_->Init()) {
    qFatal("Error initializing audio engine");
  }

  analyzer_->SetEngine(engine_);

  QObject::connect(&*engine_, &EngineBase::Error, this, &Player::Error);
  QObject::connect(&*engine_, &EngineBase::FatalError, this, &Player::FatalError);
  QObject::connect(&*engine_, &EngineBase::ValidSongRequested, this, &Player::ValidSongRequested);
  QObject::connect(&*engine_, &EngineBase::InvalidSongRequested, this, &Player::InvalidSongRequested);
  QObject::connect(&*engine_, &EngineBase::StateChanged, this, &Player::EngineStateChanged);
  QObject::connect(&*engine_, &EngineBase::TrackAboutToEnd, this, &Player::TrackAboutToEnd);
  QObject::connect(&*engine_, &EngineBase::TrackEnded, this, &Player::TrackEnded);
  QObject::connect(&*engine_, &EngineBase::MetaData, this, &Player::EngineMetadataReceived);
  QObject::connect(&*engine_, &EngineBase::VolumeChanged, this, &Player::SetVolumeFromEngine);

  // Equalizer
  QObject::connect(&*equalizer_, &Equalizer::StereoBalancerEnabledChanged, &*engine_, &EngineBase::SetStereoBalancerEnabled);
  QObject::connect(&*equalizer_, &Equalizer::StereoBalanceChanged, &*engine_, &EngineBase::SetStereoBalance);
  QObject::connect(&*equalizer_, &Equalizer::EqualizerEnabledChanged, &*engine_, &EngineBase::SetEqualizerEnabled);
  QObject::connect(&*equalizer_, &Equalizer::EqualizerParametersChanged, &*engine_, &EngineBase::SetEqualizerParameters);

  engine_->SetStereoBalancerEnabled(equalizer_->is_stereo_balancer_enabled());
  engine_->SetStereoBalance(equalizer_->stereo_balance());
  engine_->SetEqualizerEnabled(equalizer_->is_equalizer_enabled());
  engine_->SetEqualizerParameters(equalizer_->preamp_value(), equalizer_->gain_values());

  ReloadSettings();

  LoadVolume();

}

void Player::ReloadSettings() {

  Settings s;

  s.beginGroup(PlaylistSettings::kSettingsGroup);
  continue_on_error_ = s.value("continue_on_error", false).toBool();
  greyout_ = s.value("greyout_songs_play", true).toBool();
  s.endGroup();

  s.beginGroup(BehaviourSettings::kSettingsGroup);
  menu_previousmode_ = static_cast<BehaviourSettings::PreviousBehaviour>(s.value(BehaviourSettings::kMenuPreviousMode, static_cast<int>(BehaviourSettings::PreviousBehaviour::DontRestart)).toInt());
  seek_step_sec_ = s.value(BehaviourSettings::kSeekStepSec, 10).toInt();
  volume_increment_ = s.value(BehaviourSettings::kVolumeIncrement, 5).toUInt();
  s.endGroup();

  engine_->ReloadSettings();

}

void Player::UrlHandlerRegistered(UrlHandler *url_handler) const {

  QObject::connect(url_handler, &UrlHandler::AsyncLoadComplete, this, &Player::HandleLoadResult);

}

void Player::LoadVolume() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  const uint volume = s.value(kVolume, 100).toUInt();
  s.endGroup();

  SetVolume(volume);

}

void Player::SaveVolume() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kVolume, volume_);
  s.endGroup();

}

void Player::SavePlaybackStatus() {

  Settings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kPlaybackState, static_cast<int>(GetState()));
  if (GetState() == EngineBase::State::Playing || GetState() == EngineBase::State::Paused) {
    s.setValue(kPlaybackPlaylist, playlist_manager_->active()->id());
    s.setValue(kPlaybackPosition, engine_->position_nanosec() / kNsecPerSec);
  }
  else {
    s.setValue(kPlaybackPlaylist, -1);
    s.setValue(kPlaybackPosition, 0);
  }
  s.endGroup();

}

void Player::PlaylistsLoaded() {

  playlists_loaded_ = true;

  Settings s;

  s.beginGroup(BehaviourSettings::kSettingsGroup);
  const bool resume_playback = s.value("resumeplayback", false).toBool();
  s.endGroup();

  s.beginGroup(kSettingsGroup);
  const EngineBase::State playback_state = static_cast<EngineBase::State>(s.value(kPlaybackState, static_cast<int>(EngineBase::State::Empty)).toInt());
  s.endGroup();

  if (resume_playback && (playback_state == EngineBase::State::Playing || playback_state == EngineBase::State::Paused)) {
    ResumePlayback();
  }
  else if (play_requested_) {
    Play();
  }

  play_requested_ = false;

}

void Player::ResumePlayback() {

  qLog(Debug) << "Resuming playback";

  Settings s;
  s.beginGroup(kSettingsGroup);
  const EngineBase::State playback_state = static_cast<EngineBase::State>(s.value(kPlaybackState, static_cast<int>(EngineBase::State::Empty)).toInt());
  const int playback_playlist = s.value(kPlaybackPlaylist, -1).toInt();
  const quint64 playback_position = s.value(kPlaybackPosition, 0).toULongLong();
  s.endGroup();

  if (playback_playlist == playlist_manager_->current()->id()) {
    // Set active to current to resume playback on correct playlist.
    playlist_manager_->SetActiveToCurrent();
    if (playback_state == EngineBase::State::Playing) {
      Play(playback_position * kNsecPerSec);
    }
    else if (playback_state == EngineBase::State::Paused) {
      PlayWithPause(playback_position * kNsecPerSec);
    }
  }

  // Reset saved playback status so we don't resume again from the same position.
  s.beginGroup(kSettingsGroup);
  s.setValue(kPlaybackState, static_cast<int>(EngineBase::State::Empty));
  s.setValue(kPlaybackPlaylist, -1);
  s.setValue(kPlaybackPosition, 0);
  s.endGroup();

}

void Player::HandleLoadResult(const UrlHandler::LoadResult &result) {

  if (loading_async_.contains(result.media_url_)) {
    loading_async_.removeAll(result.media_url_);
  }

  // Might've been an async load, so check we're still on the same item
  const int current_row = playlist_manager_->active()->current_row();
  if (current_row == -1) {
    return;
  }
  PlaylistItemPtr current_item = playlist_manager_->active()->current_item();
  if (!current_item) {
    return;
  }
  int next_row = playlist_manager_->active()->next_row();
  const bool has_next_row = next_row != -1;
  PlaylistItemPtr next_item;
  if (has_next_row) {
    next_item = playlist_manager_->active()->item_at(next_row);
  }

  bool is_current = false;
  bool is_next = false;

  if (result.media_url_ == current_item->OriginalUrl()) {
    is_current = true;
  }
  else if (has_next_row && next_item->OriginalUrl() == result.media_url_) {
    is_next = true;
  }
  else {
    return;
  }

  switch (result.type_) {
    case UrlHandler::LoadResult::Type::Error:
      if (is_current) {
        InvalidSongRequested(result.media_url_);
      }
      Q_EMIT Error(result.error_);
      break;

    case UrlHandler::LoadResult::Type::NoMoreTracks:
      qLog(Debug) << "URL handler for" << result.media_url_ << "said no more tracks" << is_current;
      if (is_current) NextItem(stream_change_type_, autoscroll_);
      break;

    case UrlHandler::LoadResult::Type::TrackAvailable:{

      qLog(Debug) << "URL handler for" << result.media_url_ << "returned" << result.stream_url_;

      Song song;
      if (is_current) song = current_item->EffectiveMetadata();
      else if (is_next) song = next_item->EffectiveMetadata();

      bool update = false;

      // Set the stream url in the temporary metadata.
      if (
        (result.stream_url_.isValid())
        &&
        (result.stream_url_ != song.effective_url())
         )
      {
        song.set_stream_url(result.stream_url_);
        update = true;
      }

      // If there was no filetype in the song's metadata, use the one provided by URL handler, if there is one.
      if (
        (song.filetype() == Song::FileType::Unknown && result.filetype_ != Song::FileType::Unknown)
        ||
        (song.filetype() == Song::FileType::Stream && result.filetype_ != Song::FileType::Stream)
         )
      {
        song.set_filetype(result.filetype_);
        update = true;
      }

      // If there was no samplerate info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.samplerate() <= 0 && result.samplerate_ > 0) {
        song.set_samplerate(result.samplerate_);
        update = true;
      }

      // If there was no bit depth info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.bitdepth() <= 0 && result.bit_depth_ > 0) {
        song.set_bitdepth(result.bit_depth_);
        update = true;
      }

      // If there was no length info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.length_nanosec() <= 0 && result.length_nanosec_ != -1) {
        song.set_length_nanosec(result.length_nanosec_);
        update = true;
      }

      if (update) {
        if (is_current) {
          playlist_manager_->active()->UpdateItemMetadata(current_row, current_item, song, true);
        }
        else if (is_next) {
          playlist_manager_->active()->UpdateItemMetadata(next_row, next_item, song, true);
        }
      }

      if (is_current) {
        qLog(Debug) << "Playing song" << current_item->EffectiveMetadata().title() << result.stream_url_ << "position" << play_offset_nanosec_;
        engine_->Play(result.media_url_, result.stream_url_, pause_, stream_change_type_, song.has_cue(), static_cast<quint64>(song.beginning_nanosec()), song.end_nanosec(), play_offset_nanosec_, song.ebur128_integrated_loudness_lufs());
        current_item_ = current_item;
        play_offset_nanosec_ = 0;
      }
      else if (is_next && !current_item->EffectiveMetadata().is_module_music()) {
        qLog(Debug) << "Preloading next song" << next_item->EffectiveMetadata().title() << result.stream_url_;
        engine_->StartPreloading(next_item->OriginalUrl(), result.stream_url_, song.has_cue(), song.beginning_nanosec(), song.end_nanosec());
      }

      break;
    }

    case UrlHandler::LoadResult::Type::WillLoadAsynchronously:
      qLog(Debug) << "URL handler for" << result.media_url_ << "is loading asynchronously";

      // We'll get called again later with either NoMoreTracks or TrackAvailable
      loading_async_ << result.media_url_;
      break;
  }

}

void Player::Next() { NextInternal(EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Always); }

void Player::NextInternal(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  if (HandleStopAfter(autoscroll)) return;

  NextItem(change, autoscroll);

}

void Player::NextItem(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  Playlist *active_playlist = playlist_manager_->active();

  // If we received too many errors in auto change, with repeat enabled, we stop
  if (change & EngineBase::TrackChangeType::Auto) {
    const PlaylistSequence::RepeatMode repeat_mode = active_playlist->RepeatMode();
    if (repeat_mode != PlaylistSequence::RepeatMode::Off) {
      if ((repeat_mode == PlaylistSequence::RepeatMode::Track && nb_errors_received_ >= 3) || (nb_errors_received_ >= playlist_manager_->active()->filter()->rowCount())) {
        // We received too many "Error" state changes: probably looping over a playlist which contains only unavailable elements: stop now.
        nb_errors_received_ = 0;
        Stop();
        return;
      }
    }
  }

  if (nb_errors_received_ >= 100) {
    Stop();
    return;
  }

  // Manual track changes override "Repeat track"
  const bool ignore_repeat_track = change & EngineBase::TrackChangeType::Manual;

  int i = active_playlist->next_row(ignore_repeat_track);
  if (i == -1) {
    playlist_manager_->active()->set_current_row(i);
    playlist_manager_->active()->reset_last_played();
    Q_EMIT PlaylistFinished();
    Stop();
    return;
  }

  PlayAt(i, false, 0, change, autoscroll, false, true);

}

void Player::PlayPlaylist(const QString &playlist_name) {
  PlayPlaylistInternal(EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Always, playlist_name);
}

void Player::PlayPlaylistInternal(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const QString &playlist_name) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  Playlist *playlist = nullptr;
  const QList<Playlist*> playlists = playlist_manager_->GetAllPlaylists();
  for (Playlist *p : playlists) {
    if (playlist_name == playlist_manager_->GetPlaylistName(p->id())) {
      playlist = p;
      break;
    }
  }

  if (playlist == nullptr) {
    qLog(Warning) << "Playlist '" << playlist_name << "' not found.";
    return;
  }

  playlist_manager_->SetActivePlaylist(playlist->id());
  playlist_manager_->SetCurrentPlaylist(playlist->id());
  if (playlist->rowCount() == 0) return;

  int i = playlist_manager_->active()->current_row();
  if (i == -1) i = playlist_manager_->active()->last_played_row();
  if (i == -1) i = 0;

  PlayAt(i, false, 0, change, autoscroll, true);

}

bool Player::HandleStopAfter(const Playlist::AutoScroll autoscroll) {

  if (playlist_manager_->active()->stop_after_current()) {
    // Find what the next track would've been, and mark that one as current, so it plays next time the user presses Play.
    const int next_row = playlist_manager_->active()->next_row();
    if (next_row != -1) {
      playlist_manager_->active()->set_current_row(next_row, autoscroll, true);
    }

    playlist_manager_->active()->StopAfter(-1);

    Stop(true);
    return true;
  }

  return false;

}

void Player::TrackEnded() {

  if (current_item_ && current_item_->IsLocalCollectionItem() && current_item_->EffectiveMetadata().id() != -1) {
    playlist_manager_->collection_backend()->IncrementPlayCountAsync(current_item_->EffectiveMetadata().id());
  }

  if (HandleStopAfter(Playlist::AutoScroll::Maybe)) return;

  NextInternal(EngineBase::TrackChangeType::Auto, Playlist::AutoScroll::Maybe);

}

void Player::PlayPause(const quint64 offset_nanosec, const Playlist::AutoScroll autoscroll) {

  switch (engine_->state()) {
    case EngineBase::State::Paused:
      UnPause();
      Q_EMIT Resumed();
      break;

    case EngineBase::State::Playing:{
      if (current_item_->options() & PlaylistItem::Option::PauseDisabled) {
        Stop();
      }
      else {
        pause_time_ = QDateTime::currentDateTime();
        play_offset_nanosec_ = static_cast<quint64>(engine_->position_nanosec());
        engine_->Pause();
      }
      break;
    }

    case EngineBase::State::Empty:
    case EngineBase::State::Error:
    case EngineBase::State::Idle:{
      pause_time_ = QDateTime();
      play_offset_nanosec_ = offset_nanosec;
      playlist_manager_->SetActivePlaylist(playlist_manager_->current_id());
      if (playlist_manager_->active()->rowCount() == 0) break;
      int i = playlist_manager_->active()->current_row();
      if (i == -1) i = playlist_manager_->active()->last_played_row();
      if (i == -1) i = 0;
      PlayAt(i, false, offset_nanosec, EngineBase::TrackChangeType::First, autoscroll, true);
      break;
    }
  }

}

void Player::UnPause() {

  if (current_item_ && pause_time_.isValid()) {
    const Song &song = current_item_->EffectiveMetadata();
    if (url_handlers_->CanHandle(song.url()) && song.stream_url_can_expire()) {
      const qint64 time = QDateTime::currentSecsSinceEpoch() - pause_time_.toSecsSinceEpoch();
      if (time >= 30) {  // Stream URL might be expired.
        qLog(Debug) << "Re-requesting stream URL for" << song.url();
        play_offset_nanosec_ = static_cast<quint64>(engine_->position_nanosec());
        UrlHandler *url_handler = url_handlers_->GetUrlHandler(song.url());
        HandleLoadResult(url_handler->StartLoading(song.url()));
        return;
      }
    }
  }

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  engine_->Unpause();

}

void Player::RestartOrPrevious() {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  if (engine_->position_nanosec() < 8 * kNsecPerSec) {
    Previous();
    return;
  }

  SeekTo(0);

}

void Player::Stop(const bool stop_after) {

  engine_->Stop(stop_after);
  playlist_manager_->active()->set_current_row(-1);
  playlist_manager_->active()->reset_played_indexes();
  current_item_.reset();
  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

}

void Player::StopAfterCurrent() {
  playlist_manager_->active()->StopAfter(playlist_manager_->active()->current_row());
}

bool Player::PreviousWouldRestartTrack() const {

  // Check if it has been over two seconds since previous button was pressed
  return menu_previousmode_ == BehaviourSettings::PreviousBehaviour::Restart && last_pressed_previous_.isValid() && last_pressed_previous_.secsTo(QDateTime::currentDateTime()) >= 2;

}

void Player::Previous() { PreviousItem(EngineBase::TrackChangeType::Manual); }

void Player::PreviousItem(const EngineBase::TrackChangeFlags change) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  const bool ignore_repeat_track = change & EngineBase::TrackChangeType::Manual;

  if (menu_previousmode_ == BehaviourSettings::PreviousBehaviour::Restart) {
    // Check if it has been over two seconds since previous button was pressed
    QDateTime now = QDateTime::currentDateTime();
    if (last_pressed_previous_.isValid() && last_pressed_previous_.secsTo(now) >= 2) {
      last_pressed_previous_ = now;
      PlayAt(playlist_manager_->active()->current_row(), false, 0, change, Playlist::AutoScroll::Always, false, true);
      return;
    }
    last_pressed_previous_ = now;
  }

  int i = playlist_manager_->active()->previous_row(ignore_repeat_track);
  playlist_manager_->active()->set_current_row(i, Playlist::AutoScroll::Always, false);
  if (i == -1) {
    Stop();
    PlayAt(i, false, 0, change, Playlist::AutoScroll::Always, true);
    return;
  }

  PlayAt(i, false, 0, change, Playlist::AutoScroll::Always, false);

}

void Player::EngineStateChanged(const EngineBase::State state) {

  if (state == EngineBase::State::Error) {
    nb_errors_received_++;
  }
  else {
    nb_errors_received_ = 0;
  }

  switch (state) {
    case EngineBase::State::Paused:
      pause_time_ = QDateTime::currentDateTime();
      play_offset_nanosec_ = static_cast<quint64>(engine_->position_nanosec());
      Q_EMIT Paused();
      break;
    case EngineBase::State::Playing:
      pause_time_ = QDateTime();
      play_offset_nanosec_ = 0;
      Q_EMIT Playing();
      break;
    case EngineBase::State::Error:
      Q_EMIT Error();
      [[fallthrough]];
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
      pause_time_ = QDateTime();
      play_offset_nanosec_ = 0;
      Q_EMIT Stopped();
      break;
  }

  last_state_ = state;

}

uint Player::GetVolume() const {

  return engine_->volume();

}

void Player::SetVolumeFromSlider(const int value) {

  const uint volume = static_cast<uint>(qBound(0, value, 100));
  if (volume != volume_) {
    volume_ = volume;
    engine_->SetVolume(volume);
    Q_EMIT VolumeChanged(volume);
    timer_save_volume_->start();
  }

}

void Player::SetVolumeFromEngine(const uint volume) {

  const uint new_volume = qBound(0U, volume, 100U);
  if (new_volume != volume_) {
    volume_ = new_volume;
    Q_EMIT VolumeChanged(new_volume);
    timer_save_volume_->start();
  }

}

void Player::SetVolume(const uint volume) {

  const uint new_volume = qBound(0U, volume, 100U);
  if (new_volume != volume_) {
    volume_ = new_volume;
    engine_->SetVolume(new_volume);
    Q_EMIT VolumeChanged(new_volume);
    timer_save_volume_->start();
  }

}

void Player::VolumeUp() {

  uint old_volume = GetVolume();
  uint new_volume = std::min(old_volume + volume_increment_, static_cast<uint>(100));
  if (new_volume == old_volume) return;
  SetVolume(new_volume);

}

void Player::VolumeDown() {

  uint old_volume = GetVolume();
  uint new_volume = static_cast<uint>(std::max(static_cast<int>(old_volume) - static_cast<int>(volume_increment_), 0));
  if (new_volume == old_volume) return;
  SetVolume(new_volume);

}

void Player::PlayAt(const int index, const bool pause, const quint64 offset_nanosec, EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform) {

  pause_time_ = pause ? QDateTime::currentDateTime() : QDateTime();
  play_offset_nanosec_ = offset_nanosec;

  if (current_item_ && change & EngineBase::TrackChangeType::Manual && engine_->position_nanosec() != engine_->length_nanosec()) {
    Q_EMIT TrackSkipped(current_item_);
  }

  if (current_item_ && playlist_manager_->active()->has_item_at(index) && current_item_->EffectiveMetadata().IsOnSameAlbum(playlist_manager_->active()->item_at(index)->EffectiveMetadata())) {
    change |= EngineBase::TrackChangeType::SameAlbum;
  }

  if (reshuffle) playlist_manager_->active()->ReshuffleIndices();

  playlist_manager_->active()->set_current_row(index, autoscroll, false, force_inform);
  if (playlist_manager_->active()->current_row() == -1) {
    // Maybe index didn't exist in the playlist.
    return;
  }

  current_item_ = playlist_manager_->active()->current_item();
  const QUrl url = current_item_->EffectiveUrl();

  if (url_handlers_->CanHandle(url)) {
    // It's already loading
    if (loading_async_.contains(url)) {
      return;
    }

    pause_ = pause;
    stream_change_type_ = change;
    autoscroll_ = autoscroll;
    UrlHandler *url_handler = url_handlers_->GetUrlHandler(url);
    HandleLoadResult(url_handler->StartLoading(url));
  }
  else {
    qLog(Debug) << "Playing song" << current_item_->EffectiveMetadata().title() << url << "position" << offset_nanosec;
    engine_->Play(current_item_->OriginalUrl(), url, pause, change, current_item_->EffectiveMetadata().has_cue(), static_cast<quint64>(current_item_->effective_beginning_nanosec()), current_item_->effective_end_nanosec(), offset_nanosec, current_item_->EffectiveMetadata().ebur128_integrated_loudness_lufs());
  }

}

void Player::CurrentMetadataChanged(const Song &metadata) {

  // Those things might have changed (especially when a previously invalid song was reloaded) so we push the latest version into Engine
  engine_->RefreshMarkers(static_cast<quint64>(metadata.beginning_nanosec()), metadata.end_nanosec());

}

void Player::SeekTo(const quint64 seconds) {

  const qint64 length_nanosec = engine_->length_nanosec();

  // If the length is 0 then either there is no song playing, or the song isn't seekable.
  if (length_nanosec <= 0) {
    return;
  }

  const qint64 nanosec = qBound(0LL, static_cast<qint64>(seconds) * kNsecPerSec, length_nanosec);
  engine_->Seek(static_cast<quint64>(nanosec));

  qLog(Debug) << "Track seeked to" << nanosec << "ns - updating scrobble point";
  playlist_manager_->active()->UpdateScrobblePoint(nanosec);

  Q_EMIT Seeked(nanosec / 1000);

  if (seconds == 0) {
    playlist_manager_->active()->InformOfCurrentSongChange(false);
  }

}

void Player::SeekForward() {
  SeekTo(static_cast<quint64>(engine()->position_nanosec() / kNsecPerSec + seek_step_sec_));
}

void Player::SeekBackward() {
  SeekTo(static_cast<quint64>(engine()->position_nanosec() / kNsecPerSec - seek_step_sec_));
}

void Player::EngineMetadataReceived(const EngineMetadata &engine_metadata) {

  if (engine_metadata.type == EngineMetadata::Type::Any || engine_metadata.type == EngineMetadata::Type::Current) {
    const int current_row = playlist_manager_->active()->current_row();
    if (current_row != -1) {
      PlaylistItemPtr item = playlist_manager_->active()->current_item();
      if (item && engine_metadata.media_url == item->OriginalUrl()) {
        Song song = item->EffectiveMetadata();
        song.MergeFromEngineMetadata(engine_metadata);
        playlist_manager_->active()->UpdateItemMetadata(current_row, item, song, true);
        return;
      }
    }
  }

  if (engine_metadata.type == EngineMetadata::Type::Any || engine_metadata.type == EngineMetadata::Type::Next) {
    const int next_row = playlist_manager_->active()->next_row();
    if (next_row != -1) {
      PlaylistItemPtr next_item = playlist_manager_->active()->item_at(next_row);
      if (engine_metadata.media_url == next_item->OriginalUrl()) {
        Song song = next_item->EffectiveMetadata();
        song.MergeFromEngineMetadata(engine_metadata);
        playlist_manager_->active()->UpdateItemMetadata(next_row, next_item, song, true);
      }
    }
  }

}

PlaylistItemPtr Player::GetItemAt(const int pos) const {

  if (pos < 0 || pos >= playlist_manager_->active()->rowCount())
    return PlaylistItemPtr();
  return playlist_manager_->active()->item_at(pos);

}

void Player::Mute() {

  const uint current_volume = engine_->volume();

  if (current_volume == 0) {
    SetVolume(volume_before_mute_);
  }
  else {
    volume_before_mute_ = current_volume;
    SetVolume(0);
  }

}

void Player::Pause() { engine_->Pause(); }

void Player::Play(const quint64 offset_nanosec) {

  if (!playlists_loaded_) {
    play_requested_ = true;
    return;
  }

  switch (GetState()) {
    case EngineBase::State::Playing:
      SeekTo(offset_nanosec);
      break;
    case EngineBase::State::Paused:
      UnPause();
      break;
    default:
      PlayPause(offset_nanosec);
      break;
  }

}

void Player::PlayWithPause(const quint64 offset_nanosec) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = offset_nanosec;
  playlist_manager_->SetActivePlaylist(playlist_manager_->current_id());
  if (playlist_manager_->active()->rowCount() == 0) return;
  int i = playlist_manager_->active()->current_row();
  if (i == -1) i = playlist_manager_->active()->last_played_row();
  if (i == -1) i = 0;
  PlayAt(i, true, offset_nanosec, EngineBase::TrackChangeType::First, Playlist::AutoScroll::Always, true);

}

void Player::ShowOSD() {
  if (current_item_) Q_EMIT ForceShowOSD(current_item_->EffectiveMetadata(), false);
}

void Player::TogglePrettyOSD() {
  if (current_item_) Q_EMIT ForceShowOSD(current_item_->EffectiveMetadata(), true);
}

void Player::TrackAboutToEnd() {

  const bool has_next_row = playlist_manager_->active()->next_row() != -1;
  PlaylistItemPtr next_item;

  if (has_next_row) {
    next_item = playlist_manager_->active()->item_at(playlist_manager_->active()->next_row());
  }

  if (engine_->is_autocrossfade_enabled()) {
    // Crossfade is on, so just start playing the next track.  The current one will fade out, and the new one will fade in

    // If the decoding failed, current_item_ will be null
    if (!current_item_) return;

    // But, if there's no next track, and we don't want to fade out, then do nothing and just let the track finish to completion.
    if (!engine_->is_fadeout_enabled() && !has_next_row) return;

    // If the next track is on the same album (or same cue file),
    // and the user doesn't want to crossfade between tracks on the same album, then don't do this automatic crossfading.
    if (engine_->crossfade_same_album() || !has_next_row || !next_item || !current_item_->EffectiveMetadata().IsOnSameAlbum(next_item->EffectiveMetadata())) {
      TrackEnded();
      return;
    }
  }

  // Crossfade is off, so start preloading the next track, so we don't get a gap between songs.
  if (!has_next_row || !next_item) return;

  QUrl url = next_item->EffectiveUrl();

  // Get the actual track URL rather than the stream URL.
  if (url_handlers_->CanHandle(url)) {
    if (loading_async_.contains(url)) return;
    autoscroll_ = Playlist::AutoScroll::Maybe;
    UrlHandler *url_handler = url_handlers_->GetUrlHandler(url);
    const UrlHandler::LoadResult result = url_handler->StartLoading(url);
    switch (result.type_) {
      case UrlHandler::LoadResult::Type::Error:
        Q_EMIT Error(result.error_);
        return;
      case UrlHandler::LoadResult::Type::NoMoreTracks:
        return;
      case UrlHandler::LoadResult::Type::WillLoadAsynchronously:
        loading_async_ << url;
        return;
      case UrlHandler::LoadResult::Type::TrackAvailable:
        qLog(Debug) << "URL handler for" << result.media_url_ << "returned" << result.stream_url_;
        url = result.stream_url_;
        Song song = next_item->EffectiveMetadata();
        song.set_stream_url(url);
        next_item->SetStreamMetadata(song);
        break;
    }
  }

  // Preloading any format while currently playing module music is broken in GStreamer.
  // See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/769
  if (current_item_ && current_item_->EffectiveMetadata().is_module_music()) {
    return;
  }

  engine_->StartPreloading(next_item->OriginalUrl(), url, next_item->EffectiveMetadata().has_cue(), next_item->effective_beginning_nanosec(), next_item->effective_end_nanosec());

}

void Player::FatalError() {
  nb_errors_received_ = 0;
  Stop();
}

void Player::ValidSongRequested(const QUrl &url) {
  Q_EMIT SongChangeRequestProcessed(url, true);
}

void Player::InvalidSongRequested(const QUrl &url) {

  if (greyout_) Q_EMIT SongChangeRequestProcessed(url, false);

  if (!continue_on_error_) {
    FatalError();
    return;
  }

  NextItem(EngineBase::TrackChangeType::Auto, Playlist::AutoScroll::Maybe);

}

void Player::HandleAuthentication() {
  Q_EMIT Authenticated();
}
