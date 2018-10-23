/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QMap>
#include <QString>
#include <QtDebug>

#include "core/logging.h"
#include "internetservices.h"
#include "internetservice.h"

InternetServices::InternetServices(QObject *parent) : QObject(parent) {}
InternetServices::~InternetServices() {}

void InternetServices::AddService(InternetService *service) {

  services_.insert(service->source(), service);
  connect(service, SIGNAL(destroyed()), SLOT(ServiceDeleted()));
  if (service->has_initial_load_settings()) service->InitialLoadSettings();
  else service->ReloadSettings();

  qLog(Debug) << "Added internet service" << service->name();

}

void InternetServices::RemoveService(InternetService *service) {

  if (!services_.contains(service->source())) return;
  services_.remove(service->source());
  disconnect(service, 0, this, 0);

}

void InternetServices::ServiceDeleted() {

  InternetService *service = qobject_cast<InternetService*>(sender());
  if (service) RemoveService(service);

}

InternetService *InternetServices::ServiceBySource(const Song::Source &source) {

  if (services_.contains(source)) return services_.value(source);
  return nullptr;

}

void InternetServices::ReloadSettings() {
  for (InternetService *service : services_.values()) {
    service->ReloadSettings();
  }
}
