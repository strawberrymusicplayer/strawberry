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

#include <QList>
#include <QVariant>
#include <QString>
#include <QSettings>
#include <QtDebug>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"
#include "settings/settingsdialog.h"
#include "settings/scrobblersettingspage.h"

#include "audioscrobbler.h"
#include "scrobblerservices.h"
#include "scrobblerservice.h"
#include "lastfmscrobbler.h"
#include "librefmscrobbler.h"
#include "listenbrainzscrobbler.h"

AudioScrobbler::AudioScrobbler(Application *app, QObject *parent) :
  QObject(parent),
  app_(app),
  scrobbler_services_(new ScrobblerServices(this)),
  enabled_(false),
  offline_(false),
  scrobble_button_(false),
  love_button_(false),
  submit_delay_(0),
  prefer_albumartist_(false),
  show_error_dialog_(false)
  {

  scrobbler_services_->AddService(new LastFMScrobbler(app_, scrobbler_services_));
  scrobbler_services_->AddService(new LibreFMScrobbler(app_, scrobbler_services_));
  scrobbler_services_->AddService(new ListenBrainzScrobbler(app_, scrobbler_services_));

  ReloadSettings();

  for (ScrobblerService *service : scrobbler_services_->List()) {
    connect(service, SIGNAL(ErrorMessage(QString)), SLOT(ErrorReceived(QString)));
  }

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
  s.endGroup();

  emit ScrobblingEnabledChanged(enabled_);
  emit ScrobbleButtonVisibilityChanged(scrobble_button_);
  emit LoveButtonVisibilityChanged(love_button_);

  for (ScrobblerService *service : scrobbler_services_->List()) {
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
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Scrobbler);
}

void AudioScrobbler::UpdateNowPlaying(const Song &song) {

  qLog(Debug) << "Sending now playing for song" << song.artist() << song.album() << song.title();

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled()) continue;
    service->UpdateNowPlaying(song);
  }

}

void AudioScrobbler::ClearPlaying() {

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled()) continue;
    service->ClearPlaying();
  }

}

void AudioScrobbler::Scrobble(const Song &song, const int scrobble_point) {

  qLog(Debug) << "Scrobbling song" << song.artist() << song.album() << song.title() << "at" << scrobble_point;

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled()) continue;
    service->Scrobble(song);
  }

}

void AudioScrobbler::Love() {

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled() || !service->IsAuthenticated()) continue;
    service->Love();
  }

}

void AudioScrobbler::Submit() {

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled() || !service->IsAuthenticated() || service->IsSubmitted()) continue;
    service->DoSubmit();
  }

}

void AudioScrobbler::WriteCache() {

  for (ScrobblerService *service : scrobbler_services_->List()) {
    if (!service->IsEnabled()) continue;
    service->WriteCache();
  }

}

void AudioScrobbler::ErrorReceived(QString error) {
  emit ErrorMessage(error);
}
