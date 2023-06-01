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

#ifndef PULSEDEVICEFINDER_H
#define PULSEDEVICEFINDER_H

#include "config.h"

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>

#include "devicefinder.h"
#include "enginedevice.h"

class PulseDeviceFinder : public DeviceFinder {
 public:
  explicit PulseDeviceFinder();
  ~PulseDeviceFinder() override;

  bool Initialize() override;
  EngineDeviceList ListDevices() override;

 private:
  struct ListDevicesState {
    ListDevicesState() : finished(false) {}

    bool finished;
    EngineDeviceList devices;
  };

  bool Reconnect();

  static void GetSinkInfoCallback(pa_context *c, const pa_sink_info *info, int eol, void *state_voidptr);

  pa_mainloop *mainloop_;
  pa_context *context_;

  Q_DISABLE_COPY(PulseDeviceFinder)
};

#endif  // PULSEDEVICEFINDER_H
