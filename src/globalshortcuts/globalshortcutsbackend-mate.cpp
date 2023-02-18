/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QAction>

#include "core/logging.h"
#include "globalshortcutsmanager.h"
#include "globalshortcutsbackend.h"
#include "globalshortcutsbackend-mate.h"

#include "matesettingsdaemon.h"

const char *GlobalShortcutsBackendMate::kService1 = "org.mate.SettingsDaemon.MediaKeys";
const char *GlobalShortcutsBackendMate::kService2 = "org.mate.SettingsDaemon";
const char *GlobalShortcutsBackendMate::kPath = "/org/mate/SettingsDaemon/MediaKeys";

GlobalShortcutsBackendMate::GlobalShortcutsBackendMate(GlobalShortcutsManager *manager, QObject *parent)
    : GlobalShortcutsBackend(manager, GlobalShortcutsBackend::Type::Mate, parent),
      interface_(nullptr),
      is_connected_(false) {}

bool GlobalShortcutsBackendMate::IsAvailable() const {

  return IsMateAvailable();

}

bool GlobalShortcutsBackendMate::IsMateAvailable() {

  return QDBusConnection::sessionBus().interface()->isServiceRegistered(kService1) || QDBusConnection::sessionBus().interface()->isServiceRegistered(kService2);

}

bool GlobalShortcutsBackendMate::DoRegister() {

  qLog(Debug) << "Registering";

  if (!interface_) {
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(kService1)) {
      interface_ = new OrgMateSettingsDaemonMediaKeysInterface(kService1, kPath, QDBusConnection::sessionBus(), this);
    }
    else if (QDBusConnection::sessionBus().interface()->isServiceRegistered(kService2)) {
      interface_ = new OrgMateSettingsDaemonMediaKeysInterface(kService2, kPath, QDBusConnection::sessionBus(), this);
    }
  }

  if (!interface_) {
    qLog(Warning) << "Mate settings daemon not registered";
    return false;
  }

  QDBusPendingReply<> reply = interface_->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toSecsSinceEpoch());

  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &GlobalShortcutsBackendMate::RegisterFinished);

  return true;

}

void GlobalShortcutsBackendMate::RegisterFinished(QDBusPendingCallWatcher *watcher) {

  QDBusMessage reply = watcher->reply();
  watcher->deleteLater();

  if (reply.type() == QDBusMessage::ErrorMessage) {
    qLog(Warning) << "Failed to grab media keys" << reply.errorName() << reply.errorMessage();
    return;
  }

  QObject::connect(interface_, &OrgMateSettingsDaemonMediaKeysInterface::MediaPlayerKeyPressed, this, &GlobalShortcutsBackendMate::MateMediaKeyPressed);
  is_connected_ = true;

  qLog(Debug) << "Registered.";

}

void GlobalShortcutsBackendMate::DoUnregister() {

  qLog(Debug) << "Unregister";

  if (!IsAvailable() || !interface_ || !is_connected_) return;

  is_connected_ = false;

  interface_->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
  QObject::disconnect(interface_, &OrgMateSettingsDaemonMediaKeysInterface::MediaPlayerKeyPressed, this, &GlobalShortcutsBackendMate::MateMediaKeyPressed);

}

void GlobalShortcutsBackendMate::MateMediaKeyPressed(const QString&, const QString &key) {

  auto shortcuts = manager_->shortcuts();
  if (key == "Play") shortcuts["play_pause"].action->trigger();
  if (key == "Stop") shortcuts["stop"].action->trigger();
  if (key == "Next") shortcuts["next_track"].action->trigger();
  if (key == "Previous") shortcuts["prev_track"].action->trigger();

}
