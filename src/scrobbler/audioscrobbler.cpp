/*
 * Strawberry Music Player
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

#include <QMutex>
#include <QList>
#include <QString>
#include <QSettings>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"
#include "settings/settingsdialog.h"
#include "settings/scrobblersettingspage.h"

#include "audioscrobbler.h"
#include "scrobblerservice.h"
#include "lastfmscrobbler.h"
#include "librefmscrobbler.h"
#include "listenbrainzscrobbler.h"
#ifdef HAVE_SUBSONIC
#  include "subsonicscrobbler.h"
#endif

AudioScrobbler::AudioScrobbler(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      enabled_(false),
      offline_(false),
      scrobble_button_(false),
      love_button_(false),
      submit_delay_(0),
      prefer_albumartist_(false),
      show_error_dialog_(false) {

  ReloadSettings();

}

AudioScrobbler::~AudioScrobbler() {

  while (!services_.isEmpty()) {
    delete services_.take(services_.firstKey());
  }

}

void AudioScrobbler::AddService(ScrobblerService *service) {

  {
    QMutexLocker locker(&mutex_);
    services_.insert(service->name(), service);
  }

  QObject::connect(service, &ScrobblerService::ErrorMessage, this, &AudioScrobbler::ErrorReceived);

  qLog(Debug) << "Registered scrobbler service" << service->name();

}

void AudioScrobbler::RemoveService(ScrobblerService *service) {

  if (!service || !services_.contains(service->name())) return;

  {
    QMutexLocker locker(&mutex_);
    services_.remove(service->name());
    QObject::disconnect(service, nullptr, this, nullptr);
  }

  QObject::disconnect(service, &ScrobblerService::ErrorMessage, this, &AudioScrobbler::ErrorReceived);

  qLog(Debug) << "Unregistered scrobbler service" << service->name();

}

int AudioScrobbler::NextId() { return next_id_.fetchAndAddRelaxed(1); }

QList<ScrobblerService*> AudioScrobbler::GetAll() {

  QList<ScrobblerService*> services;

  {
    QMutexLocker locker(&mutex_);
    services = services_.values();
  }

  return services;

}

ScrobblerService *AudioScrobbler::ServiceByName(const QString &name) {

  if (services_.contains(name)) return services_.value(name);
  return nullptr;

}

void AudioScrobbler::ReloadSettings() {

  QSettings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  enabled_ = s.value("enabled", false).toBool();
  offline_ = s.value("offline", false).toBool();
  scrobble_button_ = s.value("scrobble_button", false).toBool();
  love_button_ = s.value("love_button", false).toBool();
  submit_delay_ = s.value("submit", 0).toInt();
  prefer_albumartist_ = s.value("albumartist", false).toBool();
  show_error_dialog_ = s.value("show_error_dialog", true).toBool();
  QStringList sources = s.value("sources").toStringList();
  s.endGroup();

  sources_.clear();

  if (sources.isEmpty()) {
    sources_ << Song::Source::Unknown
             << Song::Source::LocalFile
             << Song::Source::Collection
             << Song::Source::CDDA
             << Song::Source::Device
             << Song::Source::Stream
             << Song::Source::Tidal
             << Song::Source::Subsonic
             << Song::Source::Qobuz
             << Song::Source::SomaFM
             << Song::Source::RadioParadise;
  }
  else {
    for (const QString &source : sources) {
      sources_ << Song::SourceFromText(source);
    }
  }

  emit ScrobblingEnabledChanged(enabled_);
  emit ScrobbleButtonVisibilityChanged(scrobble_button_);
  emit LoveButtonVisibilityChanged(love_button_);

  QList<ScrobblerService*> services = services_.values();
  for (ScrobblerService *service : services) {
    service->ReloadSettings();
  }

}

void AudioScrobbler::ToggleScrobbling() {

  bool enabled_old_ = enabled_;
  enabled_ = !enabled_;

  QSettings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  s.setValue("enabled", enabled_);
  s.endGroup();

  if (enabled_ != enabled_old_) emit ScrobblingEnabledChanged(enabled_);
  if (enabled_ && !offline_) { Submit(); }

}

void AudioScrobbler::ToggleOffline() {

  bool offline_old_ = offline_;
  offline_ = !offline_;

  QSettings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  s.setValue("offline", offline_);
  s.endGroup();

  if (offline_ != offline_old_) { emit ScrobblingOfflineChanged(offline_); }
  if (enabled_ && !offline_) { Submit(); }

}

void AudioScrobbler::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Scrobbler);
}

void AudioScrobbler::UpdateNowPlaying(const Song &song) {

  if (!sources_.contains(song.source())) return;

  qLog(Debug) << "Sending now playing for song" << song.artist() << song.album() << song.title();

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled()) continue;
    service->UpdateNowPlaying(song);
  }

}

void AudioScrobbler::ClearPlaying() {

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled()) continue;
    service->ClearPlaying();
  }

}

void AudioScrobbler::Scrobble(const Song &song, const qint64 scrobble_point) {

  if (!sources_.contains(song.source())) return;

  qLog(Debug) << "Scrobbling song" << song.artist() << song.album() << song.title() << "at" << scrobble_point;

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled()) continue;
    service->Scrobble(song);
  }

}

void AudioScrobbler::Love() {

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled() || !service->IsAuthenticated()) continue;
    service->Love();
  }

}

void AudioScrobbler::Submit() {

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled() || !service->IsAuthenticated() || service->IsSubmitted()) continue;
    service->StartSubmit();
  }

}

void AudioScrobbler::WriteCache() {

  QList<ScrobblerService*> services = GetAll();
  for (ScrobblerService *service : services) {
    if (!service->IsEnabled()) continue;
    service->WriteCache();
  }

}

void AudioScrobbler::ErrorReceived(const QString &error) {
  emit ErrorMessage(error);
}
