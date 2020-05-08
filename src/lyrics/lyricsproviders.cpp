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

#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QVariantList>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QtDebug>

#include "core/logging.h"
#include "lyricsprovider.h"
#include "lyricsproviders.h"

#include "settings/lyricssettingspage.h"

int LyricsProviders::NextOrderId = 0;

LyricsProviders::LyricsProviders(QObject *parent) : QObject(parent) {}

LyricsProviders::~LyricsProviders() {

  while (!lyrics_providers_.isEmpty()) {
    delete lyrics_providers_.firstKey();
  }

}

void LyricsProviders::ReloadSettings() {

  QMap<int, QString> all_providers;
  for (LyricsProvider *provider : lyrics_providers_.keys()) {
    if (!provider->is_enabled()) continue;
    all_providers.insert(provider->order(), provider->name());
  }

  QSettings s;
  s.beginGroup(LyricsSettingsPage::kSettingsGroup);
  QStringList providers_enabled = s.value("providers", QStringList() << all_providers.values()).toStringList();
  s.endGroup();

  int i = 0;
  QList<LyricsProvider*> providers;
  for (const QString &name : providers_enabled) {
    LyricsProvider *provider = ProviderByName(name);
    if (provider) {
      provider->set_enabled(true);
      provider->set_order(++i);
      providers << provider;
    }
  }

  for (LyricsProvider *provider : lyrics_providers_.keys()) {
    if (!providers.contains(provider)) {
      provider->set_enabled(false);
      provider->set_order(++i);
    }
  }

}

LyricsProvider *LyricsProviders::ProviderByName(const QString &name) const {

  for (LyricsProvider *provider : lyrics_providers_.keys()) {
    if (provider->name() == name) return provider;
  }
  return nullptr;

}

void LyricsProviders::AddProvider(LyricsProvider *provider) {

  {
    QMutexLocker locker(&mutex_);
    lyrics_providers_.insert(provider, provider->name());
    connect(provider, SIGNAL(destroyed()), SLOT(ProviderDestroyed()));
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
