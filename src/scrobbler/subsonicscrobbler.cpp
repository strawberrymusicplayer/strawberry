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

#include <QtGlobal>
#include <QDesktopServices>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QMessageBox>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/logging.h"
#include "core/closure.h"
#include "internet/localredirectserver.h"
#include "internet/internetservices.h"
#include "subsonic/subsonicservice.h"

#include "audioscrobbler.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"
#include "subsonicscrobbler.h"

const char *SubsonicScrobbler::kName = "Subsonic";

SubsonicScrobbler::SubsonicScrobbler(Application *app, QObject *parent) : ScrobblerService(kName, app, parent),
  app_(app),
  service_(app->internet_services()->Service<SubsonicService>()),
  submitted_(false) {
  ReloadSettings();
}

SubsonicScrobbler::~SubsonicScrobbler() {

}

void SubsonicScrobbler::ReloadSettings() {

}

void SubsonicScrobbler::UpdateNowPlaying(const Song &song) {
  if (song.source() != Song::Source::Source_Subsonic) return;

  song_playing_ = song;

  if (!song.is_metadata_good() || app_->scrobbler()->IsOffline()) return;

  service_->Scrobble(song.song_id(), false);
}

void SubsonicScrobbler::ClearPlaying() {
  song_playing_ = Song();
}

void SubsonicScrobbler::Scrobble(const Song &song) {
  if (song.source() != Song::Source::Source_Subsonic) return;

  if (song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  if (app_->scrobbler()->IsOffline()) return;

  if (!submitted_) {
    submitted_ = true;
    if (app_->scrobbler()->SubmitDelay() <= 0) {
      Submit();
    }
    else {
      qint64 msec = (app_->scrobbler()->SubmitDelay() * 60 * kMsecPerSec);
      DoAfter(this, SLOT(Submit()), msec);
    }
  }

}

void SubsonicScrobbler::DoSubmit() {

}

void SubsonicScrobbler::Submit() {
  qLog(Debug) << "SubsonicScrobbler: Submitting scrobble for " << song_playing_.song_id();
  submitted_ = false;

  if (app_->scrobbler()->IsOffline()) return;

  service_->Scrobble(song_playing_.song_id(), true);
}

void SubsonicScrobbler::Error(const QString &error, const QVariant &debug) {
  qLog(Error) << "SubsonicScrobbler:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}

