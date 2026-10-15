/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/operation.h>

#include <QtGlobal>
#include <QVariant>
#include <QString>

#include "core/logging.h"
#include "pulseaudiodevicelister.h"
#include "enginedevice.h"

using namespace Qt::Literals::StringLiterals;

PulseAudioDeviceLister::PulseAudioDeviceLister() : AudioDeviceLister(u"pulseaudio"_s, { u"pulseaudio"_s, u"pulse"_s, u"pulsesink"_s }), mainloop_(nullptr), context_(nullptr) {}

bool PulseAudioDeviceLister::Initialize() {

  mainloop_ = pa_mainloop_new();
  if (!mainloop_) {
    qLog(Warning) << "Failed to create pulseaudio mainloop";
    return false;
  }

  return Reconnect();
}

bool PulseAudioDeviceLister::Reconnect() {

  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }

  context_ = pa_context_new(pa_mainloop_get_api(mainloop_), "Strawberry audio device lister");
  if (!context_) {
    qLog(Warning) << "Failed to create pulseaudio context";
    return false;
  }

  if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    qLog(Warning) << "Failed to connect pulseaudio context";
    pa_context_unref(context_);
    context_ = nullptr;
    return false;
  }

  // Wait for the context to be connected.
  Q_FOREVER {
    const pa_context_state state = pa_context_get_state(context_);
    if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
      qLog(Warning) << "Connection to pulseaudio failed";
      pa_context_disconnect(context_);
      pa_context_unref(context_);
      context_ = nullptr;
      return false;
    }

    if (state == PA_CONTEXT_READY) {
      return true;
    }

    pa_mainloop_iterate(mainloop_, true, nullptr);
  }

}

EngineDeviceList PulseAudioDeviceLister::ListDevices() {

  // Reconnect when the connection was lost, for example when pulseaudio was restarted since the last time.
  if (!context_ || pa_context_get_state(context_) == PA_CONTEXT_FAILED || pa_context_get_state(context_) == PA_CONTEXT_TERMINATED) {
    if (!mainloop_ || !Reconnect()) {
      return EngineDeviceList();
    }
  }

  if (pa_context_get_state(context_) != PA_CONTEXT_READY) {
    return EngineDeviceList();
  }

  // Try again once after reconnecting, in case pulseaudio was restarted.
  for (int attempt = 0; attempt < 2; ++attempt) {

    ListDevicesState state;
    pa_operation *operation = pa_context_get_sink_info_list(context_, &PulseAudioDeviceLister::GetSinkInfoCallback, &state);
    if (!operation) {
      qLog(Warning) << "Failed to list pulseaudio sinks:" << pa_strerror(pa_context_errno(context_));
      return EngineDeviceList();
    }

    // Wait until all sinks are listed, the operation failed or the connection was lost.
    while (!state.finished && pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
      if (pa_context_get_state(context_) != PA_CONTEXT_READY || pa_mainloop_iterate(mainloop_, true, nullptr) < 0) {
        break;
      }
    }

    // Check the connection after the loop, losing the connection cancels the operation, which also ends the loop.
    const pa_context_state context_state = pa_context_get_state(context_);
    const bool connection_lost = !state.finished && (context_state == PA_CONTEXT_FAILED || context_state == PA_CONTEXT_TERMINATED);

    // Cancel the operation if it's still running, so the callback is not called with the state after it's gone, and release it.
    if (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
      pa_operation_cancel(operation);
    }
    pa_operation_unref(operation);

    if (!connection_lost) {
      return state.devices;
    }

    // Maybe pulseaudio died. Try reconnecting.
    if (!Reconnect()) {
      return state.devices;
    }

  }

  return EngineDeviceList();

}

void PulseAudioDeviceLister::GetSinkInfoCallback(pa_context *c, const pa_sink_info *info, int eol, void *state_voidptr) {

  ListDevicesState *state = reinterpret_cast<ListDevicesState*>(state_voidptr);
  if (!state) return;

  if (info) {
    EngineDevice device;
    device.description = QString::fromUtf8(info->description);
    device.value = QString::fromUtf8(info->name);
    device.iconname = device.GuessIconName();

    state->devices.append(device);
  }

  // eol is negative when listing the sinks failed, the callback is not called again then.
  if (eol < 0) {
    qLog(Warning) << "Failed to list pulseaudio sinks:" << pa_strerror(pa_context_errno(c));
  }
  if (eol != 0) {
    state->finished = true;
  }

}

PulseAudioDeviceLister::~PulseAudioDeviceLister() {

  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }

  if (mainloop_) {
    pa_mainloop_free(mainloop_);
  }

}
