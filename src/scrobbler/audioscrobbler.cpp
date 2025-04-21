/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <memory>

#include <QList>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/song.h"

#include "audioscrobbler.h"
#include "scrobblersettingsservice.h"
#include "scrobblerservice.h"

using std::make_shared;

AudioScrobbler::AudioScrobbler(QObject *parent)
    : QObject(parent),
      settings_(make_shared<ScrobblerSettingsService>()) {

  ReloadSettings();

}

AudioScrobbler::~AudioScrobbler() {

  while (!services_.isEmpty()) {
    ScrobblerServicePtr service = services_.value(services_.firstKey());
    RemoveService(service);
  }

}

void AudioScrobbler::AddService(ScrobblerServicePtr service) {

  services_.insert(service->name(), service);

  QObject::connect(&*service, &ScrobblerService::ErrorMessage, this, &AudioScrobbler::ErrorReceived);

  qLog(Debug) << "Registered scrobbler service" << service->name();

}

void AudioScrobbler::RemoveService(ScrobblerServicePtr service) {

  if (!service || !services_.contains(service->name())) return;

  services_.remove(service->name());
  QObject::disconnect(&*service, nullptr, this, nullptr);

  QObject::disconnect(&*service, &ScrobblerService::ErrorMessage, this, &AudioScrobbler::ErrorReceived);

  qLog(Debug) << "Unregistered scrobbler service" << service->name();

}

QList<ScrobblerServicePtr> AudioScrobbler::GetAll() {

  return services_.values();

}

ScrobblerServicePtr AudioScrobbler::ServiceByName(const QString &name) {

  if (services_.contains(name)) return services_.value(name);
  return nullptr;

}

void AudioScrobbler::ReloadSettings() {

  settings_->ReloadSettings();

  const QList<ScrobblerServicePtr> services = services_.values();
  for (ScrobblerServicePtr service : std::as_const(services)) {
    service->ReloadSettings();
  }

}

void AudioScrobbler::ToggleScrobbling() {

  settings_->ToggleScrobbling();

  if (settings_->enabled() && !settings_->offline()) { Submit(); }

}

void AudioScrobbler::ToggleOffline() {

  settings_->ToggleOffline();

  if (settings_->enabled() && !settings_->offline()) { Submit(); }

}

void AudioScrobbler::UpdateNowPlaying(const Song &song) {

  if (!settings_->sources().contains(song.source())) return;

  qLog(Debug) << "Sending now playing for song" << song.artist() << song.album() << song.title();

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled()) continue;
    service->UpdateNowPlaying(song);
  }

}

void AudioScrobbler::ClearPlaying() {

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled()) continue;
    service->ClearPlaying();
  }

}

void AudioScrobbler::Scrobble(const Song &song, const qint64 scrobble_point) {

  if (!settings_->sources().contains(song.source())) return;

  qLog(Debug) << "Scrobbling song" << song.artist() << song.album() << song.title() << "at" << scrobble_point;

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled()) continue;
    service->Scrobble(song);
  }

}

void AudioScrobbler::Love() {

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled() || !service->authenticated()) continue;
    service->Love();
  }

}

void AudioScrobbler::Submit() {

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled() || !service->authenticated() || service->submitted()) continue;
    service->StartSubmit();
  }

}

void AudioScrobbler::WriteCache() {

  const QList<ScrobblerServicePtr> services = GetAll();
  for (ScrobblerServicePtr service : services) {
    if (!service->enabled()) continue;
    service->WriteCache();
  }

}

void AudioScrobbler::ErrorReceived(const QString &error) {
  Q_EMIT ErrorMessage(error);
}
