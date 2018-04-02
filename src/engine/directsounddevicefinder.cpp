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

#ifdef INTERFACE
#undef INTERFACE
#endif

#include "config.h"

#include <dsound.h>

#include <QUuid>

#include "directsounddevicefinder.h"

DirectSoundDeviceFinder::DirectSoundDeviceFinder()
    : DeviceFinder("directsoundsink") {
}

QList<DeviceFinder::Device> DirectSoundDeviceFinder::ListDevices() {
  State state;
  DirectSoundEnumerateA(&DirectSoundDeviceFinder::EnumerateCallback, &state);
  return state.devices;
}

BOOL DirectSoundDeviceFinder::EnumerateCallback(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID state_voidptr) {

  State *state = reinterpret_cast<State*>(state_voidptr);

  if (guid) {
    Device dev;
    dev.description = QString::fromUtf8(description);
    dev.device_property_value = QUuid(*guid).toByteArray();
    dev.iconname = GuessIconName(dev.string, dev.description);
    state->devices.append(dev);
  }

  return 1;

}

