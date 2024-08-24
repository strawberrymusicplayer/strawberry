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
#include "streamingservices.h"
#include "streamingservice.h"

StreamingServices::StreamingServices(QObject *parent) : QObject(parent) {}

StreamingServices::~StreamingServices() {

  while (!services_.isEmpty()) {
    StreamingServicePtr service = services_.value(services_.firstKey());
    RemoveService(service);
  }

}

void StreamingServices::AddService(StreamingServicePtr service) {

  services_.insert(service->source(), service);
  if (service->has_initial_load_settings()) service->InitialLoadSettings();
  else service->ReloadSettings();

  qLog(Debug) << "Added streaming service" << service->name();

}

void StreamingServices::RemoveService(StreamingServicePtr service) {

  if (!services_.contains(service->source())) return;
  services_.remove(service->source());
  QObject::disconnect(&*service, nullptr, this, nullptr);

  qLog(Debug) << "Removed streaming service" << service->name();

}

StreamingServicePtr StreamingServices::ServiceBySource(const Song::Source source) const {

  if (services_.contains(source)) return services_.value(source);
  return nullptr;

}

void StreamingServices::ReloadSettings() {

  const QList<StreamingServicePtr> services = services_.values();
  for (StreamingServicePtr service : services) {
    service->ReloadSettings();
  }

}

void StreamingServices::Exit() {

  const QList<StreamingServicePtr> services = services_.values();
  for (StreamingServicePtr service : services) {
    wait_for_exit_ << &*service;
    QObject::connect(&*service, &StreamingService::ExitFinished, this, &StreamingServices::ExitReceived);
    service->Exit();
  }
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void StreamingServices::ExitReceived() {

  StreamingService *service = qobject_cast<StreamingService*>(sender());
  wait_for_exit_.removeAll(service);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}
