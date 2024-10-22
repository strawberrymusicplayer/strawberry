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

#ifndef STREAMINGSERVICES_H
#define STREAMINGSERVICES_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class StreamingService;

class StreamingServices : public QObject {
  Q_OBJECT

 public:
  explicit StreamingServices(QObject *parent = nullptr);
  ~StreamingServices() override;

  SharedPtr<StreamingService> ServiceBySource(const Song::Source source) const;

  template <typename T>
  SharedPtr<T> Service() {
    return std::static_pointer_cast<T>(ServiceBySource(T::kSource));
  }

  void AddService(SharedPtr<StreamingService> service);
  void RemoveService(SharedPtr<StreamingService> service);
  void ReloadSettings();
  void Exit();

 Q_SIGNALS:
  void ExitFinished();

 private Q_SLOTS:
  void ExitReceived();

 private:
  QMap<Song::Source, SharedPtr<StreamingService>> services_;
  QList<StreamingService*> wait_for_exit_;
};

#endif  // STREAMINGSERVICES_H
