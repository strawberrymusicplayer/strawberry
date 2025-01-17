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

#include <utility>
#include <memory>

#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QSettings>

#include "core/logging.h"
#include "core/settings.h"
#include "core/networkaccessmanager.h"

#include "lyricsprovider.h"
#include "lyricsproviders.h"

#include "constants/lyricssettings.h"

int LyricsProviders::NextOrderId = 0;

using std::make_shared;

LyricsProviders::LyricsProviders(QObject *parent) : QObject(parent), thread_(new QThread(this)), network_(make_shared<NetworkAccessManager>()) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));
  thread_->setObjectName(objectName());
  network_->moveToThread(thread_);
  thread_->start();

}

LyricsProviders::~LyricsProviders() {

  while (!lyrics_providers_.isEmpty()) {
    delete lyrics_providers_.firstKey();
  }

  thread_->quit();
  thread_->wait(1000);

}

void LyricsProviders::ReloadSettings() {

  QMap<int, QString> all_providers;
  QList<LyricsProvider*> old_providers = lyrics_providers_.keys();
  for (LyricsProvider *provider : std::as_const(old_providers)) {
    if (!provider->is_enabled()) continue;
    all_providers.insert(provider->order(), provider->name());
  }

  Settings s;
  s.beginGroup(LyricsSettings::kSettingsGroup);
  const QStringList providers_enabled = s.value(LyricsSettings::kProviders, QStringList() << all_providers.values()).toStringList();
  s.endGroup();

  int i = 0;
  QList<LyricsProvider*> new_providers;
  for (const QString &name : providers_enabled) {
    LyricsProvider *provider = ProviderByName(name);
    if (provider) {
      provider->set_enabled(true);
      provider->set_order(++i);
      new_providers << provider;  // clazy:exclude=reserve-candidates
    }
  }

  old_providers = lyrics_providers_.keys();
  for (LyricsProvider *provider : std::as_const(old_providers)) {
    if (!new_providers.contains(provider)) {
      provider->set_enabled(false);
      provider->set_order(++i);
    }
  }

}

LyricsProvider *LyricsProviders::ProviderByName(const QString &name) const {

  const QList<LyricsProvider*> providers = lyrics_providers_.keys();
  for (LyricsProvider *provider : providers) {
    if (provider->name() == name) return provider;
  }
  return nullptr;

}

void LyricsProviders::AddProvider(LyricsProvider *provider) {

  provider->moveToThread(thread_);

  {
    QMutexLocker locker(&mutex_);
    lyrics_providers_.insert(provider, provider->name());
    QObject::connect(provider, &LyricsProvider::destroyed, this, &LyricsProviders::ProviderDestroyed);
  }

  provider->set_order(++NextOrderId);

  qLog(Debug) << "Registered lyrics provider" << provider->name();

}

void LyricsProviders::RemoveProvider(LyricsProvider *provider) {

  if (!provider) return;

  // It's not safe to dereference provider at this point because it might have already been destroyed.

  QString name;

  {
    QMutexLocker locker(&mutex_);
    name = lyrics_providers_.take(provider);
  }

  if (name.isNull()) {
    qLog(Debug) << "Tried to remove a lyrics provider that was not registered";
  }
  else {
    qLog(Debug) << "Unregistered lyrics provider" << name;
  }

}

void LyricsProviders::ProviderDestroyed() {

  LyricsProvider *provider = static_cast<LyricsProvider*>(sender());
  RemoveProvider(provider);

}

int LyricsProviders::NextId() { return next_id_.fetchAndAddRelaxed(1); }
