/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef COVERPROVIDERS_H
#define COVERPROVIDERS_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>
#include <QAtomicInt>

class CoverProvider;

// This is a repository for cover providers.
// Providers are automatically unregistered from the repository when they are deleted.  The class is thread safe.
class CoverProviders : public QObject {
  Q_OBJECT

 public:
  explicit CoverProviders(QObject *parent = nullptr);
  ~CoverProviders() override;

  void ReloadSettings();

  CoverProvider *ProviderByName(const QString &name) const;

  // Lets a cover provider register itself in the repository.
  void AddProvider(CoverProvider *provider);
  void RemoveProvider(CoverProvider *provider);

  // Returns a list of cover providers
  QList<CoverProvider*> List() const { return cover_providers_.keys(); }

  // Returns true if this repository has at least one registered provider.
  bool HasAnyProviders() const { return !cover_providers_.isEmpty(); }

  int NextId();

 private Q_SLOTS:
  void ProviderDestroyed();

 private:
  Q_DISABLE_COPY(CoverProviders)

  static int NextOrderId;

  QMap<CoverProvider*, QString> cover_providers_;
  QMutex mutex_;

  QAtomicInt next_id_;
};

#endif  // COVERPROVIDERS_H
