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

#ifndef SCROBBLERSERVICES_H
#define SCROBBLERSERVICES_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>
#include <QAtomicInt>

class ScrobblerService;

class ScrobblerServices : public QObject {
  Q_OBJECT

 public:
  explicit ScrobblerServices(QObject *parent = nullptr);
  ~ScrobblerServices();

  void AddService(ScrobblerService *service);
  void RemoveService(ScrobblerService *service);
  QList<ScrobblerService*> List() const { return scrobbler_services_.values(); }
  bool HasAnyServices() const { return !scrobbler_services_.isEmpty(); }
  int NextId();

  ScrobblerService *ServiceByName(const QString &name);
  template <typename T>
  T *Service() {
    return static_cast<T*>(this->ServiceByName(T::kName));
  }

 private:
  Q_DISABLE_COPY(ScrobblerServices)

  QMap<QString, ScrobblerService *> scrobbler_services_;
  QMutex mutex_;

  QAtomicInt next_id_;

};

#endif  // SCROBBLERSERVICES_H
