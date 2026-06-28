/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GSTBUSMESSAGEEVENT_H
#define GSTBUSMESSAGEEVENT_H

#include "config.h"

#include <gst/gst.h>

#include <QtGlobal>
#include <QEvent>

// Carries a GstMessage from the GLib thread to the pipeline's own thread when the bus watch does not already run there (Windows/macOS).
// The message is reference-counted for the lifetime of the event, so it stays valid until handled and is released even if Qt discards the event during teardown.
// The generation identifies the bus-watch session the message was posted under, so the receiver can drop messages left over from a torn-down or replaced watch.
class GstBusMessageEvent : public QEvent {
 public:
  static QEvent::Type EventType();

  GstBusMessageEvent(GstMessage *message, const quint64 generation);
  ~GstBusMessageEvent() override;

  GstMessage *message() const { return message_; }
  quint64 generation() const { return generation_; }

 private:
  GstMessage *message_;
  quint64 generation_;
};

#endif  // GSTBUSMESSAGEEVENT_H
