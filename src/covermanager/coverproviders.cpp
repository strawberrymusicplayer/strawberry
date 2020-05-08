/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include "coverprovider.h"
#include "coverproviders.h"

#include "settings/coverssettingspage.h"

int CoverProviders::NextOrderId = 0;

CoverProviders::CoverProviders(QObject *parent) : QObject(parent) {}

CoverProviders::~CoverProviders() {

  while (!cover_providers_.isEmpty()) {
    delete cover_providers_.firstKey();
  }

}

void CoverProviders::ReloadSettings() {

  QMap<int, QString> all_providers;
  for (CoverProvider *provider : cover_providers_.keys()) {
    if (!provider->is_enabled()) continue;
    all_providers.insert(provider->order(), provider->name());
  }

  QSettings s;
  s.beginGroup(CoversSettingsPage::kSettingsGroup);
  QStringList providers_enabled = s.value("providers", QStringList() << all_providers.values()).toStringList();
  s.endGroup();

  int i = 0;
  QList<CoverProvider*> providers;
  for (const QString &name : providers_enabled) {
    CoverProvider *provider = ProviderByName(name);
    if (provider) {
      provider->set_enabled(true);
      provider->set_order(++i);
      providers << provider;
    }
  }

  for (CoverProvider *provider : cover_providers_.keys()) {
    if (!providers.contains(provider)) {
      provider->set_enabled(false);
      provider->set_order(++i);
    }
  }

}

CoverProvider *CoverProviders::ProviderByName(const QString &name) const {

  for (CoverProvider *provider : cover_providers_.keys()) {
    if (provider->name() == name) return provider;
  }
  return nullptr;

}

void CoverProviders::AddProvider(CoverProvider *provider) {

  {
    QMutexLocker locker(&mutex_);
    cover_providers_.insert(provider, provider->name());
    connect(provider, SIGNAL(destroyed()), SLOT(ProviderDestroyed()));
  }

  provider->set_order(++NextOrderId);

  qLog(Debug) << "Registered cover provider" << provider->name();

}

void CoverProviders::RemoveProvider(CoverProvider *provider) {

  if (!provider) return;

  // It's not safe to dereference provider at this point because it might have already been destroyed.

  QString name;

  {
    QMutexLocker locker(&mutex_);
    name = cover_providers_.take(provider);
  }

  if (name.isNull()) {
    qLog(Debug) << "Tried to remove a cover provider that was not registered";
  }
  else {
    qLog(Debug) << "Unregistered cover provider" << name;
  }

}

void CoverProviders::ProviderDestroyed() {

  CoverProvider *provider = static_cast<CoverProvider*>(sender());
  RemoveProvider(provider);

}

int CoverProviders::NextId() { return next_id_.fetchAndAddRelaxed(1); }
