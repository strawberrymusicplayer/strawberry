/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <dbus/kglobalaccel.h>
#include <dbus/kglobalaccelcomponent.h>

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QAction>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QKeySequence>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QKeyCombination>
#endif

#include "core/logging.h"
#include "core/closure.h"

#include "globalshortcutbackend-kde.h"

const char *GlobalShortcutBackendKDE::kKdeService = "org.kde.kglobalaccel";
const char *GlobalShortcutBackendKDE::kKdePath = "/kglobalaccel";

GlobalShortcutBackendKDE::GlobalShortcutBackendKDE(GlobalShortcuts *parent) : GlobalShortcutBackend(parent), interface_(nullptr), component_(nullptr) {}

bool GlobalShortcutBackendKDE::DoRegister() {

  qLog(Debug) << "Registering";

  if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(kKdeService)) {
    qLog(Warning) << "KGlobalAccel is not registered";
    return false;
  }

  if (!interface_) {
    interface_ = new OrgKdeKGlobalAccelInterface(kKdeService, kKdePath, QDBusConnection::sessionBus(), this);
  }

  for (const GlobalShortcuts::Shortcut &shortcut : manager_->shortcuts().values()) {
    RegisterShortcut(shortcut);
  }

  QDBusPendingReply<QDBusObjectPath> reply = interface_->getComponent(QCoreApplication::applicationName());
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  NewClosure(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(RegisterFinished(QDBusPendingCallWatcher*)), watcher);

  return true;

}

void GlobalShortcutBackendKDE::RegisterFinished(QDBusPendingCallWatcher *watcher) {

  QDBusReply<QDBusObjectPath> reply = watcher->reply();
  watcher->deleteLater();

  if (!reply.isValid()) {
    if (reply.error().name() != "org.kde.kglobalaccel.NoSuchComponent") {
      qLog(Error) << "Failed to register:" << reply.error().name() << reply.error().message();
    }
    return;
  }

  if (!component_) {
    component_ = new org::kde::kglobalaccel::Component(kKdeService, reply.value().path(), QDBusConnection::sessionBus(), interface_);
  }

  if (!component_->isValid()) {
    qLog(Error) << "Component is invalid:" << QDBusConnection::sessionBus().lastError();
    return;
  }

  connect(component_, SIGNAL(globalShortcutPressed(QString, QString, qlonglong)), this, SLOT(GlobalShortcutPressed(QString, QString, qlonglong)), Qt::UniqueConnection);

  qLog(Debug) << "Registered";

}

void GlobalShortcutBackendKDE::DoUnregister() {

  if (!interface_ || !interface_->isValid()) return;

  qLog(Debug) << "Unregistering";

  for (const GlobalShortcuts::Shortcut &shortcut : manager_->shortcuts()) {
    if (actions_.contains(shortcut.id)) {
      interface_->unRegister(GetActionId(shortcut.id, shortcut.action));
      actions_.remove(shortcut.id, shortcut.action);
      qLog(Info) << "Unregistered shortcut" << shortcut.id << shortcut.action->shortcut();
    }
  }

  if (component_) disconnect(component_, nullptr, this, nullptr);

  qLog(Debug) << "Unregistered";

}

bool GlobalShortcutBackendKDE::RegisterShortcut(const GlobalShortcuts::Shortcut &shortcut) {

  if (!interface_ || !interface_->isValid() || shortcut.id.isEmpty() || !shortcut.action || shortcut.action->shortcut().isEmpty()) return false;

  if (shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaPlay) ||
      shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaStop) ||
      shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaNext) ||
      shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaPrevious)) {
    qLog(Info) << "Media shortcut" << shortcut.id << shortcut.action->shortcut();
    return true;
  }

  QStringList action_id = GetActionId(shortcut.id, shortcut.action);
  actions_.insert(shortcut.id, shortcut.action);
  interface_->doRegister(action_id);

  QList<QKeySequence> active_shortcut = QList<QKeySequence>() << shortcut.action->shortcut();

  const QList<int> result = interface_->setShortcut(action_id, ToIntList(active_shortcut), 0x2);
  const QList<QKeySequence> result_sequence = ToKeySequenceList(result);
  if (result_sequence != active_shortcut) {
    if (result_sequence.isEmpty()) {
      shortcut.action->setShortcut(QKeySequence());
    }
    else {
      shortcut.action->setShortcut(result_sequence[0]);
    }
  }

  qLog(Info) << "Registered shortcut" << shortcut.id << shortcut.action->shortcut();

  return true;

}

QStringList GlobalShortcutBackendKDE::GetActionId(const QString &id, const QAction *action) {

  QStringList ret;
  ret << QCoreApplication::applicationName();
  ret << id;
  ret << QCoreApplication::applicationName();
  ret << action->text().remove('&');
  if (ret.back().isEmpty()) ret.back() = id;

  return ret;

}

QList<int> GlobalShortcutBackendKDE::ToIntList(const QList<QKeySequence> &sequence_list) {

  QList<int> ret;
  for (const QKeySequence &sequence : sequence_list) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ret.append(sequence[0].toCombined());
#else
    ret.append(sequence[0]);
#endif
  }

  return ret;

}

QList<QKeySequence> GlobalShortcutBackendKDE::ToKeySequenceList(const QList<int> &sequence_list) {

  QList<QKeySequence> ret;
  for (int sequence : sequence_list) {
    ret.append(sequence);
  }

  return ret;

}

void GlobalShortcutBackendKDE::GlobalShortcutPressed(const QString &component_unique, const QString &shortcut_unique, qlonglong) {

  if (QCoreApplication::applicationName() == component_unique && actions_.contains(shortcut_unique)) {
    for (QAction *action : actions_.values(shortcut_unique)) {
      qLog(Debug) << "Key" << action->shortcut() << "pressed.";
      if (action->isEnabled()) action->trigger();
    }
  }

}
