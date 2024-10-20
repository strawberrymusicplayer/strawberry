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

#include "config.h"

#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>

#include <QtGlobal>
#include <QVariant>
#include <QString>

#include "core/logging.h"
#include "pulsedevicefinder.h"
#include "enginedevice.h"

using namespace Qt::Literals::StringLiterals;

PulseDeviceFinder::PulseDeviceFinder() : DeviceFinder(u"pulseaudio"_s, { u"pulseaudio"_s, u"pulse"_s, u"pulsesink"_s }), mainloop_(nullptr), context_(nullptr) {}

bool PulseDeviceFinder::Initialize() {

  mainloop_ = pa_mainloop_new();
  if (!mainloop_) {
    qLog(Warning) << "Failed to create pulseaudio mainloop";
    return false;
  }

  return Reconnect();
}

bool PulseDeviceFinder::Reconnect() {

  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }

  context_ = pa_context_new(pa_mainloop_get_api(mainloop_), "Strawberry device finder");
  if (!context_) {
    qLog(Warning) << "Failed to create pulseaudio context";
    return false;
  }

  if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    qLog(Warning) << "Failed to connect pulseaudio context";
    return false;
  }

  // Wait for the context to be connected.
  Q_FOREVER {
    const pa_context_state state = pa_context_get_state(context_);
    if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
      qLog(Warning) << "Connection to pulseaudio failed";
      return false;
    }

    if (state == PA_CONTEXT_READY) {
      return true;
    }

    pa_mainloop_iterate(mainloop_, true, nullptr);
  }
}

EngineDeviceList PulseDeviceFinder::ListDevices() {

  if (!context_ || pa_context_get_state(context_) != PA_CONTEXT_READY) {
    return EngineDeviceList();
  }

retry:
  ListDevicesState state;
  pa_context_get_sink_info_list(context_, &PulseDeviceFinder::GetSinkInfoCallback, &state);

  Q_FOREVER {
    if (state.finished) {
      return state.devices;
    }

    switch (pa_context_get_state(context_)) {
      case PA_CONTEXT_READY:
        break;
      case PA_CONTEXT_FAILED:
      case PA_CONTEXT_TERMINATED:
        // Maybe pulseaudio died.  Try reconnecting.
        if (Reconnect()) {
          goto retry;
        }
        return state.devices;
      default:
        return state.devices;
    }

    pa_mainloop_iterate(mainloop_, true, nullptr);
  }
}

void PulseDeviceFinder::GetSinkInfoCallback(pa_context *c, const pa_sink_info *info, int eol, void *state_voidptr) {

  Q_UNUSED(c);

  ListDevicesState *state = reinterpret_cast<ListDevicesState*>(state_voidptr);
  if (!state) return;

  if (info) {
    EngineDevice device;
    device.description = QString::fromUtf8(info->description);
    device.value = QString::fromUtf8(info->name);
    device.iconname = device.GuessIconName();

    state->devices.append(device);
  }

  if (eol > 0) {
    state->finished = true;
  }
}

PulseDeviceFinder::~PulseDeviceFinder() {

  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }

  if (mainloop_) {
    pa_mainloop_free(mainloop_);
  }
}

