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

#include "config.h"

#include <gst/gst.h>

#include <QEvent>

#include "gstbusmessageevent.h"

QEvent::Type GstBusMessageEvent::EventType() {

  // C++ guarantees this is initialised once, so the event type is registered a single time and shared across all instances.
  static const QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
  return type;

}

GstBusMessageEvent::GstBusMessageEvent(GstMessage *message, const quint64 generation) : QEvent(EventType()), message_(gst_message_ref(message)), generation_(generation) {}

GstBusMessageEvent::~GstBusMessageEvent() { gst_message_unref(message_); }
