/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
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

#include <stdbool.h>

#include <QList>
#include <QVariant>
#include <QString>

// Finds audio output devices
class DeviceFinder {

 public:
  struct Device {
    QString description;
    QVariant value;
    QString iconname;
  };

  virtual ~DeviceFinder() {}

  // The name of the gstreamer sink element that devices found by this class can be used with.
  QString name() const { return name_; }

  // Does any necessary setup, returning false if this DeviceFinder cannot be used.
  virtual bool Initialise() = 0;

  // Returns a list of available devices.
  virtual QList<Device> ListDevices() = 0;

 protected:
  explicit DeviceFinder(const QString &name);

  static QString GuessIconName(const QString &description);

 private:
  QString name_;

};

#endif // DEVICEFINDER_H

