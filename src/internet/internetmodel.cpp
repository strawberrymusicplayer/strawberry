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
#include <QStandardItemModel>
#include <QtDebug>

#include "core/logging.h"
#include "internetmodel.h"
#include "internetservice.h"
#include "tidal/tidalservice.h"

QMap<QString, InternetService*>* InternetModel::sServices = nullptr;

InternetModel::InternetModel(Application *app, QObject *parent)
    : QStandardItemModel(parent),
      app_(app) {

  if (!sServices) sServices = new QMap<QString, InternetService*>;
  Q_ASSERT(sServices->isEmpty());
  AddService(new TidalService(app, this));

}

void InternetModel::AddService(InternetService *service) {

  qLog(Debug) << "Adding internet service:" << service->name();
  sServices->insert(service->name(), service);
  connect(service, SIGNAL(destroyed()), SLOT(ServiceDeleted()));
  if (service->has_initial_load_settings()) service->InitialLoadSettings();
  else service->ReloadSettings();

}

void InternetModel::RemoveService(InternetService *service) {

  if (!sServices->contains(service->name())) return;
  sServices->remove(service->name());
  disconnect(service, 0, this, 0);

}

void InternetModel::ServiceDeleted() {

  InternetService *service = qobject_cast<InternetService*>(sender());
  if (service) RemoveService(service);

}

InternetService *InternetModel::ServiceByName(const QString &name) {

  if (sServices->contains(name)) return sServices->value(name);
  return nullptr;

}

void InternetModel::ReloadSettings() {
  for (InternetService *service : sServices->values()) {
    service->ReloadSettings();
  }
}
