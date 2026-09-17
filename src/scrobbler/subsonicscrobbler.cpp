/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2020, Pascal Below <spezifisch@below.fr>
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
#include "constants/subsonicsettings.h"
#include "subsonic/subsonicservice.h"

#include "scrobblersettingsservice.h"
#include "scrobblerservice.h"
#include "subsonicscrobbler.h"

namespace {
constexpr char kName[] = "Subsonic";
}

SubsonicScrobbler::SubsonicScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<SubsonicService> service, QObject *parent)
    : ScrobblerService(QLatin1String(kName), network, settings, parent),
      service_(service),
      enabled_(false),
      submitted_(false),
      scrobble_pending_(false) {

  SubsonicScrobbler::ReloadSettings();

  timer_send_scrobbles_.setSingleShot(true);
  QObject::connect(&timer_send_scrobbles_, &QTimer::timeout, this, &SubsonicScrobbler::SendScrobbles);

}

void SubsonicScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(SubsonicSettings::kSettingsGroup);
  enabled_ = s.value(SubsonicSettings::kServerSideScrobbling, SubsonicSettings::kDefaultServerSideScrobbling).toBool();
  s.endGroup();

}

SubsonicServicePtr SubsonicScrobbler::service() const {

  return service_;

}

void SubsonicScrobbler::Start(const bool initial) {

  Q_UNUSED(initial)

  // Resume a scrobble that was left pending when scrobbling was switched off or offline mode was switched on.
  if (scrobble_pending_ && !submitted_) {
    ScheduleSendScrobbles();
  }

}

void SubsonicScrobbler::Stop() {

  // scrobble_pending_ is deliberately left alone, so that Start() can send the scrobble that this discards the timer for.
  timer_send_scrobbles_.stop();
  submitted_ = false;

}

void SubsonicScrobbler::UpdateNowPlaying(const Song &song) {

  if (song.source() != Song::Source::Subsonic) return;

  song_playing_ = song;
  time_ = QDateTime::currentDateTime();

  if (!song.is_metadata_good() || settings_->offline() || !service()) return;

  service()->Scrobble(song.song_id(), false, time_);

}

void SubsonicScrobbler::ClearPlaying() {

  song_playing_ = Song();
  time_ = QDateTime();

}

void SubsonicScrobbler::Scrobble(const Song &song) {

  if (song.source() != Song::Source::Subsonic || song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  if (settings_->offline()) return;

  if (!submitted_) {
    scrobble_pending_ = true;
    ScheduleSendScrobbles();
  }

}

void SubsonicScrobbler::ScheduleSendScrobbles() {

  submitted_ = true;

  if (settings_->submit_delay() <= 0) {
    SendScrobbles();
  }
  else if (!timer_send_scrobbles_.isActive()) {
    timer_send_scrobbles_.setInterval(static_cast<int>(settings_->submit_delay() * kMsecPerSec));
    timer_send_scrobbles_.start();
  }

}

void SubsonicScrobbler::SendScrobbles() {

  qLog(Debug) << "SubsonicScrobbler: Submitting scrobble for" << song_playing_.artist() << song_playing_.title();
  submitted_ = false;

  if (settings_->offline() || !service()) return;

  scrobble_pending_ = false;
  service()->Scrobble(song_playing_.song_id(), true, time_);

}

