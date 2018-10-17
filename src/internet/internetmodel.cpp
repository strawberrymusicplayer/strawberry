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
#ifdef HAVE_STREAM_TIDAL
#  include "tidal/tidalservice.h"
#endif
#ifdef HAVE_STREAM_DEEZER
#  include "deezer/deezerservice.h"
#endif

QMap<Song::Source, InternetService*>* InternetModel::sServices = nullptr;

InternetModel::InternetModel(Application *app, QObject *parent)
    : QStandardItemModel(parent),
      app_(app) {

  if (!sServices) sServices = new QMap<Song::Source, InternetService*>;
  Q_ASSERT(sServices->isEmpty());
#ifdef HAVE_STREAM_TIDAL
  AddService(new TidalService(app, this));
#endif
#ifdef HAVE_STREAM_DEEZER
  AddService(new DeezerService(app, this));
#endif

}

void InternetModel::AddService(InternetService *service) {

  qLog(Debug) << "Adding internet service:" << service->name();
  sServices->insert(service->source(), service);
  connect(service, SIGNAL(destroyed()), SLOT(ServiceDeleted()));
  if (service->has_initial_load_settings()) service->InitialLoadSettings();
  else service->ReloadSettings();

}

void InternetModel::RemoveService(InternetService *service) {

  if (!sServices->contains(service->source())) return;
  sServices->remove(service->source());
  disconnect(service, 0, this, 0);

}

void InternetModel::ServiceDeleted() {

  InternetService *service = qobject_cast<InternetService*>(sender());
  if (service) RemoveService(service);

}

InternetService *InternetModel::ServiceBySource(const Song::Source &source) {

  if (sServices->contains(source)) return sServices->value(source);
  return nullptr;

}

void InternetModel::ReloadSettings() {
  for (InternetService *service : sServices->values()) {
    service->ReloadSettings();
  }
}
