/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
 * Copyright 2017-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DEVICEFINDER_H
#define DEVICEFINDER_H

#include "config.h"

#include <QString>

#include "enginedevice.h"

// Finds audio output devices
class DeviceFinder {
 public:
  virtual ~DeviceFinder() {}

  QString name() const { return name_; }
  QStringList outputs() const { return outputs_; }
  void add_output(const QString &output) { outputs_.append(output); }

  // Does any necessary setup, returning false if this DeviceFinder cannot be used.
  virtual bool Initialize() = 0;

  // Returns a list of available devices.
  virtual EngineDeviceList ListDevices() = 0;

 protected:
  explicit DeviceFinder(const QString &name, const QStringList &outputs);

 private:
  QString name_;
  QStringList outputs_;

  Q_DISABLE_COPY(DeviceFinder)
};

#endif  // DEVICEFINDER_H
