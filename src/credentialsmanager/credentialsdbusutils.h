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

#ifndef CREDENTIALSDBUSUTILS_H
#define CREDENTIALSDBUSUTILS_H

#include <functional>

#include <QString>
#include <QDBusMessage>

class QObject;

namespace CredentialsDBusUtils {

using ReplyCallback = std::function<void(const QDBusMessage &reply)>;
using AvailableCallback = std::function<void(const bool available)>;

// Calls the method asynchronously on the session bus, the callback is called in the thread of the context object.
// The callback is not called if the context object is deleted before the reply is received.
void AsyncCall(QObject *context, const QDBusMessage &message, const ReplyCallback &callback, const int timeout = -1);

// Checks if the D-Bus service is running, or can be activated on the session bus.
void CheckServiceAvailable(QObject *context, const QString &service_name, const AvailableCallback &available_callback);

// Returns true if the reply is a reply message with at least min_arguments arguments, otherwise sets the error.
bool CheckReply(const QDBusMessage &reply, const int min_arguments, QString &error);

}  // namespace CredentialsDBusUtils

#endif  // CREDENTIALSDBUSUTILS_H
