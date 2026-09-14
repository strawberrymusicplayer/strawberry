/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <QString>
#include <QTimer>
#include <QElapsedTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "constants/timeconstants.h"
#include "constants/jellyfinsettings.h"
#include "jellyfin/jellyfinservice.h"

#include "scrobblersettingsservice.h"
#include "scrobblerservice.h"
#include "jellyfinscrobbler.h"

namespace {
constexpr char kName[] = "Jellyfin";
constexpr int kPlaybackProgressIntervalMs = 30000;
// Check the engine shortly after playback started or continued, to report the playback start when it really plays the stream, and to correct the estimated position when it seeked to the start position.
constexpr int kResyncPositionDelayMs = 2000;
constexpr int kResyncPositionMaxAttempts = 5;
}  // namespace

JellyfinScrobbler::JellyfinScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<JellyfinService> service, const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : ScrobblerService(QLatin1String(kName), network, settings, parent),
      service_(service),
      player_(player),
      enabled_(false),
      new_play_(false),
      start_pending_(false),
      playback_reported_(false),
      paused_(false),
      position_nanosec_(0),
      resync_position_attempts_(0) {

  JellyfinScrobbler::ReloadSettings();

  // The server idles out a session that has not received a check-in for a while, so periodically report the playback progress while a Jellyfin song is playing to keep it visible on the server's dashboard.
  timer_playback_progress_.setInterval(kPlaybackProgressIntervalMs);
  QObject::connect(&timer_playback_progress_, &QTimer::timeout, this, &JellyfinScrobbler::PlaybackProgressTimeout);

  timer_resync_position_.setSingleShot(true);
  timer_resync_position_.setInterval(kResyncPositionDelayMs);
  QObject::connect(&timer_resync_position_, &QTimer::timeout, this, &JellyfinScrobbler::ResyncPosition);

  if (service_) {
    QObject::connect(&*service_, &JellyfinService::ScrobbleError, this, &JellyfinScrobbler::ScrobbleError);
    QObject::connect(&*service_, &JellyfinService::PlaybackReportsCleared, this, &JellyfinScrobbler::PlaybackReportsCleared);
  }

  if (player_) {
    QObject::connect(&*player_, &Player::Paused, this, &JellyfinScrobbler::Paused);
    QObject::connect(&*player_, &Player::Resumed, this, &JellyfinScrobbler::Resumed);
    QObject::connect(&*player_, &Player::Playing, this, &JellyfinScrobbler::Playing);
    QObject::connect(&*player_, &Player::Seeked, this, &JellyfinScrobbler::Seeked);
    QObject::connect(&*player_, &Player::Stopped, this, &JellyfinScrobbler::PlayerStopped);
  }

  // The scrobbler is only told about playback while it's enabled, and only about songs with complete metadata.
  // So the current song decides whether a song is a new play, it's always told about it, before the scrobbler is.
  if (playlist_manager) {
    QObject::connect(&*playlist_manager, &PlaylistManager::CurrentSongChanged, this, &JellyfinScrobbler::CurrentSongChanged);
  }

}

void JellyfinScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(JellyfinSettings::kSettingsGroup);
  enabled_ = s.value(JellyfinSettings::kServerSideScrobbling, JellyfinSettings::kDefaultServerSideScrobbling).toBool();
  s.endGroup();

  // The scrobbler isn't told about playback while it's disabled, so stop reporting the playing track.
  CheckPlaying();

}

JellyfinServicePtr JellyfinScrobbler::service() const {

  return service_;

}

void JellyfinScrobbler::Start(const bool initial) {

  Q_UNUSED(initial)

}

void JellyfinScrobbler::Stop() {

  // The scrobbler isn't told about playback while scrobbling is switched off or offline mode is switched on, reporting starts again with the next track.
  StopReporting();

}

bool JellyfinScrobbler::IsPlayingSong() const {

  // Safety net, CurrentSongChanged() normally stops reporting when another song plays.
  if (!player_) return false;
  const PlaylistItemPtr current_item = player_->GetCurrentItem();
  if (!current_item) return false;
  const Song song = current_item->EffectiveMetadata();
  return song.source() == Song::Source::Jellyfin && song.song_id() == song_playing_.song_id();

}

bool JellyfinScrobbler::EngineHasSong() const {

  // The engine is told about the next track after the scrobbler, so check that it has this song, not the previous track.
  return player_ && player_->engine() && player_->engine()->media_url() == song_playing_.url();

}

bool JellyfinScrobbler::CheckPlaying() {

  // A song that is not reported yet is not reported when reporting starts again during it.
  if (!ReportingActive()) {
    new_play_ = false;
    start_pending_ = false;
  }

  if (!playback_reported_) return false;

  // Stop reporting when another song plays, or the scrobbler is not told about playback anymore, reporting starts again with the next track.
  if (!IsPlayingSong() || !ReportingActive()) {
    StopReporting();
    return false;
  }

  return true;

}

bool JellyfinScrobbler::ReportingActive() const {

  return enabled_ && settings_->enabled() && !settings_->offline() && settings_->sources().contains(Song::Source::Jellyfin) && service();

}

bool JellyfinScrobbler::SongChanged(const Song &song) {

  // Only a different song is a new play, the same song is not when its metadata changed, it was seeked or restarted, or it's repeated.
  const QString song_id = song.source() == Song::Source::Jellyfin ? song.song_id() : QString();
  if (song_id == current_song_id_) return false;

  // The previous song stopped playing.
  StopReporting();

  // A song that started while reporting was not active is not reported when reporting starts again during it, reporting starts again with the next track.
  current_song_id_ = song_id;
  new_play_ = !song_id.isEmpty() && ReportingActive();

  return true;

}

void JellyfinScrobbler::UpdatePosition() {

  // Only the engine knows the position of the playing track, it is estimated from the elapsed time between updates.
  // Keep the estimated position when the engine doesn't have the track yet.
  position_nanosec_ = EngineHasSong() ? player_->engine()->position_nanosec() : PositionNanosec();
  position_timer_.start();

}

qint64 JellyfinScrobbler::PositionNanosec() const {

  qint64 position_nanosec = position_nanosec_;
  if (!paused_ && position_timer_.isValid()) {
    position_nanosec += position_timer_.nsecsElapsed();
  }
  if (song_playing_.length_nanosec() > 0 && position_nanosec > song_playing_.length_nanosec()) {
    position_nanosec = song_playing_.length_nanosec();
  }

  return position_nanosec;

}

void JellyfinScrobbler::UpdateNowPlaying(const Song &song) {

  // The current song normally changed already, but don't rely on the order.
  SongChanged(song);

  if (song.source() != Song::Source::Jellyfin || song.song_id() != current_song_id_) return;

  if (new_play_) {
    // Report playback start when the engine plays the song, it can still fail to load it.
    new_play_ = false;
    start_pending_ = true;
    song_playing_ = song;
    // The engine plays a gapless track without emitting Playing, check for it.
    StartResyncPosition();
  }
  else if (playback_reported_ || start_pending_) {
    // The metadata changed, or the song was restarted or repeated.
    song_playing_ = song;
    // Correct the position with the engine, a pending start is checked already.
    if (playback_reported_) StartResyncPosition();
  }

}

void JellyfinScrobbler::StartPlayback() {

  start_pending_ = false;
  if (!ReportingActive()) return;

  // Jellyfin counts the play when playback start is reported.
  qLog(Debug) << "JellyfinScrobbler: Reporting playback start for" << song_playing_.artist() << song_playing_.title();
  service()->ReportPlaybackStart(song_playing_.song_id());
  playback_reported_ = true;
  // The engine plays the stream, the player's state is updated after it tells that it plays.
  paused_ = false;
  UpdatePosition();

  // Keep reporting the playback progress until playback stops or moves on to another track.
  timer_playback_progress_.start();

  // The playback start has no position, report it right away, playback can start at an offset.
  SendPlaybackProgress();

  // The engine can still be seeking to the start position, for example when continuing playback on startup.
  StartResyncPosition();

}

void JellyfinScrobbler::StartResyncPosition() {

  resync_position_attempts_ = 0;
  timer_resync_position_.start();

}

void JellyfinScrobbler::StopReporting() {

  new_play_ = false;
  start_pending_ = false;
  ReportStopped();
  timer_playback_progress_.stop();
  timer_resync_position_.stop();
  song_playing_ = Song();
  paused_ = false;
  position_nanosec_ = 0;
  position_timer_.invalidate();

}

void JellyfinScrobbler::ClearPlaying() {

  // Playback stopped, or the song is excluded from scrobbling.
  StopReporting();

}

void JellyfinScrobbler::Scrobble(const Song &song) {

  // Jellyfin counts the play when playback start is reported, and playback is reported as stopped when the track stops playing, not at the scrobble point.
  Q_UNUSED(song)

}

void JellyfinScrobbler::Paused() {

  if (!CheckPlaying()) return;

  // Keep the estimated position when the engine reports the start of the track, it can still be loading the stream or seeking to the start position.
  const qint64 position_nanosec = PositionNanosec();
  UpdatePosition();
  paused_ = true;
  if (position_nanosec_ == 0) {
    position_nanosec_ = position_nanosec;
    // The engine can still be seeking to the start position, for example when continuing playback paused on startup.
    StartResyncPosition();
  }
  SendPlaybackProgress();

}

void JellyfinScrobbler::Resumed() {

  if (!paused_ || !CheckPlaying()) return;

  // Continue from the position playback was paused at, the engine can still be loading the stream again after a long pause.
  // The position is resynchronized with the engine when it plays the stream.
  position_timer_.start();
  paused_ = false;
  timer_playback_progress_.start();
  StartResyncPosition();
  SendPlaybackProgress();

}

void JellyfinScrobbler::Playing() {

  // The engine reports playing before the stream is loaded, a stream that fails to load must not be reported as played.
  // Report the playback start when the engine really plays the stream, ResyncPosition() checks it.
  if (start_pending_) {
    if (EngineHasSong() && player_->engine()->state() == EngineBase::State::Playing) {
      StartPlayback();
    }
    else {
      StartResyncPosition();
    }
    return;
  }

  // Resumed is only emitted for some ways of resuming playback, Playing is emitted for all of them.
  if (paused_) Resumed();

}

void JellyfinScrobbler::ResyncPosition() {

  // A gapless track plays without the engine emitting Playing, report its start when the engine plays it.
  if (start_pending_) {
    CheckPlaying();
    if (!start_pending_) return;
    if (EngineHasSong() && player_->engine()->state() == EngineBase::State::Playing) {
      StartPlayback();
    }
    // Keep checking while the song is current, loading the stream can take a while, for example while buffering.
    // The engine's pipeline is also paused while starting, only the player's state tells that the user paused.
    // A failed stream stops playback, which clears the pending start, and while paused, Playing() checks it when playback continues.
    else if (IsPlayingSong() && player_->GetState() != EngineBase::State::Paused) {
      timer_resync_position_.start();
    }
    return;
  }

  if (!CheckPlaying()) return;

  // The engine paused by itself, for example while buffering or starting the stream again, continue when it plays again.
  if (paused_ && player_->GetState() == EngineBase::State::Playing) {
    if (EngineHasSong() && player_->engine()->state() == EngineBase::State::Playing) {
      Resumed();
    }
    else if (++resync_position_attempts_ < kResyncPositionMaxAttempts) {
      timer_resync_position_.start();
    }
    return;
  }

  // Wait until the engine plays the song, or is paused after seeking to the start position, it can still be loading the stream, but not forever.
  const bool engine_ready = EngineHasSong() && (paused_ ? player_->engine()->state() == EngineBase::State::Paused && player_->engine()->position_nanosec() > 0 : player_->engine()->state() == EngineBase::State::Playing);
  if (!engine_ready) {
    if (++resync_position_attempts_ < kResyncPositionMaxAttempts) timer_resync_position_.start();
    return;
  }

  UpdatePosition();
  if (paused_) SendPlaybackProgress();

}

void JellyfinScrobbler::CurrentSongChanged(const Song &song) {

  // The same song was restarted, repeated or its metadata changed, correct the position with the engine.
  if (!SongChanged(song) && playback_reported_) {
    StartResyncPosition();
  }

}

void JellyfinScrobbler::PlaybackReportsCleared() {

  // The server changed or Jellyfin was disabled, don't report the stop of the playing song to the new server.
  // The playing song is not a new play when reporting continues, reporting starts again with the next track.
  playback_reported_ = false;
  StopReporting();

}

void JellyfinScrobbler::PlayerStopped() {

  // Playing the song again after playback stopped is a new play, also when the scrobbler was not told about playback.
  // A song that failed to play is not reported.
  current_song_id_.clear();
  new_play_ = false;
  start_pending_ = false;

}

void JellyfinScrobbler::Seeked(const qint64 microseconds) {

  if (!CheckPlaying()) return;

  // The engine can still be seeking, so use the position that was seeked to.
  position_nanosec_ = microseconds * kNsecPerUsec;
  position_timer_.start();
  timer_playback_progress_.start();
  SendPlaybackProgress();

}

void JellyfinScrobbler::PlaybackProgressTimeout() {

  if (!CheckPlaying()) return;

  // Correct the estimated position with the engine position when the engine plays the track, it can be started at an offset or restarted without notifying the scrobbler.
  if (EngineHasSong()) {
    const EngineBase::State state = player_->engine()->state();
    if (state == EngineBase::State::Playing && !paused_) {
      UpdatePosition();
    }
    // The engine pauses and continues by itself while buffering or starting the stream again, without emitting Paused or Playing.
    // Only the player's state tells whether the user paused, check again shortly, the pause can be brief.
    else if (state == EngineBase::State::Paused && !paused_) {
      Paused();
      if (player_->GetState() == EngineBase::State::Playing) StartResyncPosition();
      return;
    }
    else if (state == EngineBase::State::Playing && paused_) {
      Resumed();
      return;
    }
  }

  SendPlaybackProgress();

}

void JellyfinScrobbler::SendPlaybackProgress() {

  if (!CheckPlaying()) return;

  qLog(Debug) << "JellyfinScrobbler: Reporting playback progress for" << song_playing_.artist() << song_playing_.title();
  service()->ReportPlaybackProgress(song_playing_.song_id(), PositionNanosec(), paused_);

}

void JellyfinScrobbler::ReportStopped() {

  // Also report the stop when offline mode was switched on, so the session doesn't stay open on the server.
  if (!playback_reported_) return;
  playback_reported_ = false;

  if (!service()) return;

  qLog(Debug) << "JellyfinScrobbler: Reporting playback stop for" << song_playing_.artist() << song_playing_.title();
  service()->ReportPlaybackStopped(song_playing_.song_id(), PositionNanosec());

}

void JellyfinScrobbler::ScrobbleError(const QString &error) {

  if (settings_->show_error_dialog()) {
    Q_EMIT ErrorMessage(tr("Scrobbler %1 error: %2").arg(name_, error));
  }

}
