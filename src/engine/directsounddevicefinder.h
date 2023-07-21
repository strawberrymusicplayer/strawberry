/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DIRECTSOUNDDEVICEFINDER_H
#define DIRECTSOUNDDEVICEFINDER_H

#include "config.h"

#include <rpc.h>

#include "devicefinder.h"
#include "enginedevice.h"

class DirectSoundDeviceFinder : public DeviceFinder {
 public:
  explicit DirectSoundDeviceFinder();

  virtual bool Initialize() { return true; }
  virtual EngineDeviceList ListDevices();

 private:
  struct State {
    EngineDeviceList devices;
  };

  static BOOL CALLBACK EnumerateCallback(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID state_voidptr);
};

#endif  // DIRECTSOUNDDEVICEFINDER_H
