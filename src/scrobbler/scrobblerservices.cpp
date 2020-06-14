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
#include <QMutexLocker>
#include <QString>
#include <QtDebug>

#include "core/logging.h"

#include "scrobblerservices.h"
#include "scrobblerservice.h"

ScrobblerServices::ScrobblerServices(QObject *parent) : QObject(parent) {}

ScrobblerServices::~ScrobblerServices() {

  while (!scrobbler_services_.isEmpty()) {
    delete scrobbler_services_.take(scrobbler_services_.firstKey());
  }

}

void ScrobblerServices::AddService(ScrobblerService *service) {

  {
    QMutexLocker locker(&mutex_);
    scrobbler_services_.insert(service->name(), service);
  }

  qLog(Debug) << "Registered scrobbler service" << service->name();

}

void ScrobblerServices::RemoveService(ScrobblerService *service) {

  if (!service || !scrobbler_services_.contains(service->name())) return;

  {
    QMutexLocker locker(&mutex_);
    scrobbler_services_.remove(service->name());
    disconnect(service, nullptr, this, nullptr);
  }

  qLog(Debug) << "Unregistered scrobbler service" << service->name();

}

int ScrobblerServices::NextId() { return next_id_.fetchAndAddRelaxed(1); }

ScrobblerService *ScrobblerServices::ServiceByName(const QString &name) {

  if (scrobbler_services_.contains(name)) return scrobbler_services_.value(name);
  return nullptr;

}
