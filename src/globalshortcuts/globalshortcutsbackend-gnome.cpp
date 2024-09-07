/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include "globalshortcutsbackend-gnome.h"

#include "gnomesettingsdaemon.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kService1[] = "org.gnome.SettingsDaemon.MediaKeys";
constexpr char kService2[] = "org.gnome.SettingsDaemon";
constexpr char kPath[] = "/org/gnome/SettingsDaemon/MediaKeys";
}  // namespace

GlobalShortcutsBackendGnome::GlobalShortcutsBackendGnome(GlobalShortcutsManager *manager, QObject *parent)
    : GlobalShortcutsBackend(manager, GlobalShortcutsBackend::Type::Gnome, parent),
      interface_(nullptr),
      is_connected_(false) {}

bool GlobalShortcutsBackendGnome::IsAvailable() const {
  return IsGnomeAvailable();
}

bool GlobalShortcutsBackendGnome::IsGnomeAvailable() {

  return QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kService1)) || QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kService2));

}

bool GlobalShortcutsBackendGnome::DoRegister() {

  qLog(Debug) << "Registering";

  if (!interface_) {
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kService1))) {
      interface_ = new OrgGnomeSettingsDaemonMediaKeysInterface(QLatin1String(kService1), QLatin1String(kPath), QDBusConnection::sessionBus(), this);
    }
    else if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kService2))) {
      interface_ = new OrgGnomeSettingsDaemonMediaKeysInterface(QLatin1String(kService2), QLatin1String(kPath), QDBusConnection::sessionBus(), this);
    }
  }

  if (!interface_) {
    qLog(Warning) << "Gnome settings daemon not registered";
    return false;
  }

  QDBusPendingReply<> reply = interface_->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentSecsSinceEpoch());

  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &GlobalShortcutsBackendGnome::RegisterFinished);

  return true;

}

void GlobalShortcutsBackendGnome::RegisterFinished(QDBusPendingCallWatcher *watcher) {

  QDBusMessage reply = watcher->reply();
  watcher->deleteLater();

  if (reply.type() == QDBusMessage::ErrorMessage) {
    qLog(Warning) << "Failed to grab media keys" << reply.errorName() << reply.errorMessage();
    return;
  }

  QObject::connect(interface_, &OrgGnomeSettingsDaemonMediaKeysInterface::MediaPlayerKeyPressed, this, &GlobalShortcutsBackendGnome::GnomeMediaKeyPressed);
  is_connected_ = true;

  qLog(Debug) << "Registered.";

}

void GlobalShortcutsBackendGnome::DoUnregister() {

  qLog(Debug) << "Unregister";

  if (!IsAvailable() || !interface_ || !is_connected_) return;

  is_connected_ = false;

  interface_->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
  QObject::disconnect(interface_, &OrgGnomeSettingsDaemonMediaKeysInterface::MediaPlayerKeyPressed, this, &GlobalShortcutsBackendGnome::GnomeMediaKeyPressed);

}

void GlobalShortcutsBackendGnome::GnomeMediaKeyPressed(const QString&, const QString &key) {

  auto shortcuts = manager_->shortcuts();
  if (key == "Play"_L1) shortcuts[QStringLiteral("play_pause")].action->trigger();
  if (key == "Stop"_L1) shortcuts[QStringLiteral("stop")].action->trigger();
  if (key == "Next"_L1) shortcuts[QStringLiteral("next_track")].action->trigger();
  if (key == "Previous"_L1) shortcuts[QStringLiteral("prev_track")].action->trigger();

}
