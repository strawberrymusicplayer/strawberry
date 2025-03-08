/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
      submitted_(false) {

  SubsonicScrobbler::ReloadSettings();

  timer_submit_.setSingleShot(true);
  QObject::connect(&timer_submit_, &QTimer::timeout, this, &SubsonicScrobbler::Submit);

}

void SubsonicScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(SubsonicSettings::kSettingsGroup);
  enabled_ = s.value("serversidescrobbling", false).toBool();
  s.endGroup();

}

SubsonicServicePtr SubsonicScrobbler::service() const {

  return service_;

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
    submitted_ = true;
    if (settings_->submit_delay() <= 0) {
      Submit();
    }
    else if (!timer_submit_.isActive()) {
      timer_submit_.setInterval(static_cast<int>(settings_->submit_delay() * kMsecPerSec));
      timer_submit_.start();
    }
  }

}

void SubsonicScrobbler::Submit() {

  qLog(Debug) << "SubsonicScrobbler: Submitting scrobble for" << song_playing_.artist() << song_playing_.title();
  submitted_ = false;

  if (settings_->offline() || !service()) return;

  service()->Scrobble(song_playing_.song_id(), true, time_);

}
