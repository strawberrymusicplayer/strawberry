/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <dbus/gnomesettingsdaemon.h>

#include <QObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QAction>
#include <QtDebug>

#include "core/closure.h"
#include "core/logging.h"
#include "globalshortcuts.h"
#include "globalshortcutbackend.h"
#include "globalshortcutbackend-gsd.h"

const char *GlobalShortcutBackendGSD::kGsdService = "org.gnome.SettingsDaemon.MediaKeys";
const char *GlobalShortcutBackendGSD::kGsdService2 = "org.gnome.SettingsDaemon";
const char *GlobalShortcutBackendGSD::kGsdPath = "/org/gnome/SettingsDaemon/MediaKeys";

GlobalShortcutBackendGSD::GlobalShortcutBackendGSD(GlobalShortcuts *parent)
    : GlobalShortcutBackend(parent),
      interface_(nullptr),
      is_connected_(false) {}

bool GlobalShortcutBackendGSD::DoRegister() {

  qLog(Debug) << "Registering";

  if (!interface_) {
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(kGsdService)) {
      interface_ = new OrgGnomeSettingsDaemonMediaKeysInterface(kGsdService, kGsdPath, QDBusConnection::sessionBus(), this);
    }
    else if (QDBusConnection::sessionBus().interface()->isServiceRegistered(kGsdService2)) {
      interface_ = new OrgGnomeSettingsDaemonMediaKeysInterface(kGsdService2, kGsdPath, QDBusConnection::sessionBus(), this);
    }
  }

  if (!interface_) {
    qLog(Warning) << "Gnome settings daemon not registered";
    return false;
  }

  QDBusPendingReply<> reply = interface_->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toTime_t());

  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  NewClosure(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(RegisterFinished(QDBusPendingCallWatcher*)), watcher);

  return true;

}

void GlobalShortcutBackendGSD::RegisterFinished(QDBusPendingCallWatcher *watcher) {

  QDBusMessage reply = watcher->reply();
  watcher->deleteLater();

  if (reply.type() == QDBusMessage::ErrorMessage) {
    qLog(Warning) << "Failed to grab media keys" << reply.errorName() <<reply.errorMessage();
    return;
  }

  connect(interface_, SIGNAL(MediaPlayerKeyPressed(QString, QString)), this, SLOT(GnomeMediaKeyPressed(QString, QString)));
  is_connected_ = true;

  qLog(Debug) << "Registered";

}

void GlobalShortcutBackendGSD::DoUnregister() {

  qLog(Debug) << "Unregister";

  // Check if the GSD service is available
  if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(kGsdService))
    return;
  if (!interface_ || !is_connected_) return;

  is_connected_ = false;

  interface_->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
  disconnect(interface_, SIGNAL(MediaPlayerKeyPressed(QString, QString)), this, SLOT(GnomeMediaKeyPressed(QString, QString)));

}

void GlobalShortcutBackendGSD::GnomeMediaKeyPressed(const QString&, const QString& key) {
  if (key == "Play") manager_->shortcuts()["play_pause"].action->trigger();
  if (key == "Stop") manager_->shortcuts()["stop"].action->trigger();
  if (key == "Next") manager_->shortcuts()["next_track"].action->trigger();
  if (key == "Previous") manager_->shortcuts()["prev_track"].action->trigger();
}
