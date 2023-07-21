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

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>

#include "core/logging.h"
#include "internetservices.h"
#include "internetservice.h"

InternetServices::InternetServices(QObject *parent) : QObject(parent) {}

InternetServices::~InternetServices() {

  while (!services_.isEmpty()) {
    InternetServicePtr service = services_.first();
    RemoveService(service);
  }

}

void InternetServices::AddService(InternetServicePtr service) {

  services_.insert(service->source(), service);
  if (service->has_initial_load_settings()) service->InitialLoadSettings();
  else service->ReloadSettings();

  qLog(Debug) << "Added internet service" << service->name();

}

void InternetServices::RemoveService(InternetServicePtr service) {

  if (!services_.contains(service->source())) return;
  services_.remove(service->source());
  QObject::disconnect(&*service, nullptr, this, nullptr);

  qLog(Debug) << "Removed internet service" << service->name();

}

InternetServicePtr InternetServices::ServiceBySource(const Song::Source source) const {

  if (services_.contains(source)) return services_.value(source);
  return nullptr;

}

void InternetServices::ReloadSettings() {

  QList<InternetServicePtr> services = services_.values();
  for (InternetServicePtr service : services) {
    service->ReloadSettings();
  }

}

void InternetServices::Exit() {

  QList<InternetServicePtr> services = services_.values();
  for (InternetServicePtr service : services) {
    wait_for_exit_ << &*service;
    QObject::connect(&*service, &InternetService::ExitFinished, this, &InternetServices::ExitReceived);
    service->Exit();
  }
  if (wait_for_exit_.isEmpty()) emit ExitFinished();

}

void InternetServices::ExitReceived() {

  InternetService *service = qobject_cast<InternetService*>(sender());
  wait_for_exit_.removeAll(service);
  if (wait_for_exit_.isEmpty()) emit ExitFinished();

}
