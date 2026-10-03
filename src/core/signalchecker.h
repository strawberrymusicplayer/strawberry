/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef SIGNALCHECKER_H
#define SIGNALCHECKER_H

#include "config.h"

#include <QtGlobal>

#include <glib.h>
#include <glib-object.h>

// Do not call this directly, use CHECKED_GCONNECT instead.
gulong CheckedGConnect(gpointer source, const char *signal, GCallback callback, gpointer data, const int callback_param_count);

// Returns the number of parameters of the callback function.
template<typename R, typename... Args>
constexpr int FunctionArity(R (*callback)(Args...)) {
  Q_UNUSED(callback)
  return static_cast<int>(sizeof...(Args));
}

template<typename R, typename... Args>
constexpr int FunctionArity(R (*callback)(Args...) noexcept) {
  Q_UNUSED(callback)
  return static_cast<int>(sizeof...(Args));
}

#define CHECKED_GCONNECT(source, signal, callback, data) CheckedGConnect(source, signal, G_CALLBACK(callback), data, FunctionArity(callback))

#endif  // SIGNALCHECKER_H
