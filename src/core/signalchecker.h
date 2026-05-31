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

#include <cstddef>

#include <glib.h>
#include <glib-object.h>

// Only the FUNCTION_ARITY / CHECKED_GCONNECT macros are part of the public API.
namespace signal_checker_detail {

// Dependent-false helper so the static_assert in the primary template only fires when the primary template is actually instantiated (i.e. when none of the specializations matched),
// rather than during the template's own definition.
template <typename> inline constexpr bool always_false_v = false;

// Compile-time arity of a function or function pointer type.
// Only plain function pointers / function types are supported.
// Member function pointers, lambdas, std::function, and other callables won't match either specialization and will fall through to the primary template,
// where the static_assert produces an actionable diagnostic at the call site instead of a cryptic "incomplete type" error.
template <typename F> struct FunctionArity {
  static_assert(always_false_v<F>, "FUNCTION_ARITY / CHECKED_GCONNECT only supports plain function pointers (e.g. &MyCallback). Member function pointers, lambdas, std::function and other callables are not supported - convert your callback to a free function or static member.");
};

template <typename R, typename... Args>
struct FunctionArity<R(Args...)> {
  static constexpr int value = static_cast<int>(sizeof...(Args));
};

template <typename R, typename... Args>
struct FunctionArity<R(*)(Args...)> {
  static constexpr int value = static_cast<int>(sizeof...(Args));
};

}  // namespace signal_checker_detail

// Do not call this directly, use CHECKED_GCONNECT instead.
gulong CheckedGConnect(gpointer source, const char *signal, GCallback callback, gpointer data, const int callback_param_count);

#define FUNCTION_ARITY(callback) ::signal_checker_detail::FunctionArity<decltype(callback)>::value

#define CHECKED_GCONNECT(source, signal, callback, data) CheckedGConnect(source, signal, G_CALLBACK(callback), data, FUNCTION_ARITY(callback))

#endif  // SIGNALCHECKER_H
