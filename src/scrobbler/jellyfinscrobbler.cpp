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

#include <memory>

#include <QVariant>
#include <QString>
#include <QDateTime>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "constants/timeconstants.h"
#include "constants/jellyfinsettings.h"
#include "jellyfin/jellyfinservice.h"

#include "scrobblersettingsservice.h"
#include "scrobblerservice.h"
#include "jellyfinscrobbler.h"

namespace {
constexpr char kName[] = "Jellyfin";
constexpr int kPlaybackProgressIntervalMs = 30000;
}

JellyfinScrobbler::JellyfinScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<JellyfinService> service, QObject *parent)
    : ScrobblerService(QLatin1String(kName), network, settings, parent),
      service_(service),
      enabled_(false),
      submitted_(false),
      scrobble_pending_(false) {

  JellyfinScrobbler::ReloadSettings();

  timer_send_scrobbles_.setSingleShot(true);
  QObject::connect(&timer_send_scrobbles_, &QTimer::timeout, this, &JellyfinScrobbler::SendScrobbles);

  // The server idles out a session that has not received a check-in for a while, so periodically report the playback progress while a Jellyfin song is playing to keep it visible on the server's dashboard.
  timer_playback_progress_.setInterval(kPlaybackProgressIntervalMs);
  QObject::connect(&timer_playback_progress_, &QTimer::timeout, this, &JellyfinScrobbler::SendPlaybackProgress);

  if (service_) {
    QObject::connect(&*service_, &JellyfinService::ScrobbleError, this, &JellyfinScrobbler::ScrobbleError);
  }

}

void JellyfinScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(JellyfinSettings::kSettingsGroup);
  enabled_ = s.value(JellyfinSettings::kServerSideScrobbling, JellyfinSettings::kDefaultServerSideScrobbling).toBool();
  s.endGroup();

}

JellyfinServicePtr JellyfinScrobbler::service() const {

  return service_;

}

void JellyfinScrobbler::Start(const bool initial) {

  Q_UNUSED(initial)

  // Resume a scrobble that was left pending when scrobbling was switched off or offline mode was switched on.
  if (scrobble_pending_ && !submitted_) {
    ScheduleSendScrobbles();
  }

}

void JellyfinScrobbler::Stop() {

  // scrobble_pending_ is deliberately left alone, so that Start() can send the scrobble that this discards the timer for.
  timer_send_scrobbles_.stop();
  timer_playback_progress_.stop();
  submitted_ = false;

}

void JellyfinScrobbler::UpdateNowPlaying(const Song &song) {

  if (song.source() != Song::Source::Jellyfin) {
    // Another source is playing now, stop reporting the Jellyfin track, a delayed report for it is kept separately.
    timer_playback_progress_.stop();
    song_playing_ = Song();
    time_ = QDateTime();
    return;
  }

  song_playing_ = song;
  time_ = QDateTime::currentDateTime();

  if (!song.is_metadata_good() || settings_->offline() || !service()) return;

  qLog(Debug) << "JellyfinScrobbler: Reporting playback start for" << song.artist() << song.title();
  service()->Scrobble(song.song_id(), false, time_);

  // Keep reporting the playback progress until playback stops or moves on to another track.
  if (!timer_playback_progress_.isActive()) {
    timer_playback_progress_.start();
  }

}

void JellyfinScrobbler::ClearPlaying() {

  timer_playback_progress_.stop();
  song_playing_ = Song();
  time_ = QDateTime();

}

void JellyfinScrobbler::Scrobble(const Song &song) {

  if (song.source() != Song::Source::Jellyfin || song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  if (settings_->offline()) return;

  // A delayed report for another track is still waiting, send it now so it is not replaced.
  if (submitted_ && timer_send_scrobbles_.isActive() && scrobble_song_.song_id() != song_playing_.song_id()) {
    timer_send_scrobbles_.stop();
    SendScrobbles();
  }

  if (!submitted_) {
    scrobble_song_ = song_playing_;
    scrobble_time_ = time_;
    scrobble_pending_ = true;
    ScheduleSendScrobbles();
  }

}

void JellyfinScrobbler::ScheduleSendScrobbles() {

  submitted_ = true;

  if (settings_->submit_delay() <= 0) {
    SendScrobbles();
  }
  else if (!timer_send_scrobbles_.isActive()) {
    timer_send_scrobbles_.setInterval(static_cast<int>(settings_->submit_delay() * kMsecPerSec));
    timer_send_scrobbles_.start();
  }

}

void JellyfinScrobbler::SendScrobbles() {

  qLog(Debug) << "JellyfinScrobbler: Submitting scrobble for" << scrobble_song_.artist() << scrobble_song_.title();
  submitted_ = false;

  if (settings_->offline() || !service()) return;

  scrobble_pending_ = false;
  if (scrobble_song_.song_id().isEmpty()) return;

  service()->Scrobble(scrobble_song_.song_id(), true, scrobble_time_);
  scrobble_song_ = Song();
  scrobble_time_ = QDateTime();

}

void JellyfinScrobbler::SendPlaybackProgress() {

  if (!time_.isValid() || song_playing_.source() != Song::Source::Jellyfin || settings_->offline() || !service()) {
    timer_playback_progress_.stop();
    return;
  }

  qLog(Debug) << "JellyfinScrobbler: Reporting playback progress for" << song_playing_.artist() << song_playing_.title();
  service()->ReportPlaybackProgress(song_playing_.song_id(), time_);

}

void JellyfinScrobbler::ScrobbleError(const QString &error) {

  if (settings_->show_error_dialog()) {
    Q_EMIT ErrorMessage(tr("Scrobbler %1 error: %2").arg(name_, error));
  }

}
