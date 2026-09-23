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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>

#include "credentialsdbusutils.h"

using namespace Qt::Literals::StringLiterals;

namespace CredentialsDBusUtils {

void AsyncCall(QObject *context, const QDBusMessage &message, const ReplyCallback &callback, const int timeout) {

  const QDBusPendingCall pending_call = QDBusConnection::sessionBus().asyncCall(message, timeout);
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending_call, context);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, callback]() {
    watcher->deleteLater();
    callback(watcher->reply());
  });

}

void CheckServiceAvailable(QObject *context, const QString &service_name, const AvailableCallback &available_callback) {

  if (!QDBusConnection::sessionBus().isConnected()) {
    available_callback(false);
    return;
  }

  QDBusMessage has_owner_message = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, u"NameHasOwner"_s);
  has_owner_message << service_name;
  AsyncCall(context, has_owner_message, [context, service_name, available_callback](const QDBusMessage &has_owner_reply) {
    const QDBusReply<bool> has_owner = has_owner_reply;
    if (has_owner.isValid() && has_owner.value()) {
      available_callback(true);
      return;
    }
    const QDBusMessage activatable_message = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, u"ListActivatableNames"_s);
    AsyncCall(context, activatable_message, [service_name, available_callback](const QDBusMessage &activatable_reply) {
      const QDBusReply<QStringList> activatable_names = activatable_reply;
      available_callback(activatable_names.isValid() && activatable_names.value().contains(service_name));
    });
  });

}

bool CheckReply(const QDBusMessage &reply, const int min_arguments, QString &error) {

  if (reply.type() == QDBusMessage::ErrorMessage) {
    error = reply.errorName() + u": "_s + reply.errorMessage();
    return false;
  }

  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().count() < min_arguments) {
    error = u"Invalid D-Bus reply."_s;
    return false;
  }

  return true;

}

}  // namespace CredentialsDBusUtils
