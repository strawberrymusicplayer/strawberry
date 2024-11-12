/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

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
#include <QKeyCombination>

#include "core/logging.h"

#include "globalshortcutsbackend-kglobalaccel.h"

#include "kglobalaccel.h"
#include "kglobalaccelcomponent.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kKGlobalAccelService[] = "org.kde.kglobalaccel";
constexpr char kKGlobalAccelPath[] = "/kglobalaccel";
}

GlobalShortcutsBackendKGlobalAccel::GlobalShortcutsBackendKGlobalAccel(GlobalShortcutsManager *manager, QObject *parent)
    : GlobalShortcutsBackend(manager, GlobalShortcutsBackend::Type::KGlobalAccel, parent),
      interface_(nullptr),
      component_(nullptr) {}

bool GlobalShortcutsBackendKGlobalAccel::IsKGlobalAccelAvailable() {

  return QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kKGlobalAccelService));

}

bool GlobalShortcutsBackendKGlobalAccel::IsAvailable() const {

  return IsKGlobalAccelAvailable();

}

bool GlobalShortcutsBackendKGlobalAccel::IsMediaShortcut(const GlobalShortcutsManager::Shortcut &shortcut) const {

  return (shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaPlay) ||
          shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaStop) ||
          shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaNext) ||
          shortcut.action->shortcut() == QKeySequence(Qt::Key_MediaPrevious));

}


bool GlobalShortcutsBackendKGlobalAccel::DoRegister() {

  qLog(Debug) << "Registering";

  if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kKGlobalAccelService))) {
    qLog(Warning) << "KGlobalAccel is not registered";
    return false;
  }

  if (!interface_) {
    interface_ = new OrgKdeKGlobalAccelInterface(QLatin1String(kKGlobalAccelService), QLatin1String(kKGlobalAccelPath), QDBusConnection::sessionBus(), this);
  }

  const QList<GlobalShortcutsManager::Shortcut> shortcuts = manager_->shortcuts().values();
  for (const GlobalShortcutsManager::Shortcut &shortcut : shortcuts) {
    RegisterShortcut(shortcut);
  }

  QDBusPendingReply<QDBusObjectPath> reply = interface_->getComponent(QCoreApplication::applicationName());
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &GlobalShortcutsBackendKGlobalAccel::RegisterFinished);

  return true;

}

void GlobalShortcutsBackendKGlobalAccel::RegisterFinished(QDBusPendingCallWatcher *watcher) {

  QDBusReply<QDBusObjectPath> reply = watcher->reply();
  watcher->deleteLater();

  if (!reply.isValid()) {
    if (reply.error().name() != u"org.kde.kglobalaccel.NoSuchComponent"_s) {
      qLog(Error) << "Failed to register:" << reply.error().name() << reply.error().message();
    }
    return;
  }

  if (!component_) {
    component_ = new org::kde::kglobalaccel::Component(QLatin1String(kKGlobalAccelService), reply.value().path(), QDBusConnection::sessionBus(), interface_);
  }

  if (!component_->isValid()) {
    qLog(Error) << "Component is invalid:" << QDBusConnection::sessionBus().lastError();
    return;
  }

  QObject::connect(component_, &org::kde::kglobalaccel::Component::globalShortcutPressed, this, &GlobalShortcutsBackendKGlobalAccel::GlobalShortcutPressed, Qt::UniqueConnection);

  qLog(Debug) << "Registered.";

}

void GlobalShortcutsBackendKGlobalAccel::DoUnregister() {

  if (!interface_ || !interface_->isValid()) return;

  qLog(Debug) << "Unregistering";

  const QMap<QString, GlobalShortcutsManager::Shortcut> shortcuts = manager_->shortcuts();
  for (const GlobalShortcutsManager::Shortcut &shortcut : shortcuts) {
    if (actions_.contains(shortcut.id)) {
      interface_->unRegister(GetActionId(shortcut.id, shortcut.action));
      actions_.remove(shortcut.id, shortcut.action);
      qLog(Info) << "Unregistered shortcut" << shortcut.id << shortcut.action->shortcut();
    }
  }

  if (component_) QObject::disconnect(component_, nullptr, this, nullptr);

  qLog(Debug) << "Unregistered";

}

bool GlobalShortcutsBackendKGlobalAccel::RegisterShortcut(const GlobalShortcutsManager::Shortcut &shortcut) {

  if (!interface_ || !interface_->isValid() || shortcut.id.isEmpty() || !shortcut.action || shortcut.action->shortcut().isEmpty()) return false;

  QStringList action_id = GetActionId(shortcut.id, shortcut.action);
  actions_.insert(shortcut.id, shortcut.action);
  interface_->doRegister(action_id);

  QList<QKeySequence> active_shortcut = QList<QKeySequence>() << shortcut.action->shortcut();

  const QList<int> result = interface_->setShortcut(action_id, ToIntList(active_shortcut), 0x2);
  const QList<QKeySequence> result_sequence = ToKeySequenceList(result);
  if (result_sequence == active_shortcut) {
    qLog(Info) << "Registered shortcut" << shortcut.id << shortcut.action->shortcut();
  }
  else {
    qLog(Error) << "KGlobalAccel returned" << result_sequence << "when setting shortcut" << active_shortcut;
    if (!result_sequence.isEmpty() && !IsMediaShortcut(shortcut)) {
      shortcut.action->setShortcut(result_sequence[0]);
    }
  }

  return true;

}

QStringList GlobalShortcutsBackendKGlobalAccel::GetActionId(const QString &id, const QAction *action) {

  QStringList ret;
  ret << QCoreApplication::applicationName();
  ret << id;
  ret << QCoreApplication::applicationName();
  ret << action->text().remove(u'&');
  if (ret.back().isEmpty()) ret.back() = id;

  return ret;

}

QList<int> GlobalShortcutsBackendKGlobalAccel::ToIntList(const QList<QKeySequence> &sequence_list) {

  QList<int> ret;
  ret.reserve(sequence_list.count());
  for (const QKeySequence &sequence : sequence_list) {
    ret.append(sequence[0].toCombined());
  }

  return ret;

}

QList<QKeySequence> GlobalShortcutsBackendKGlobalAccel::ToKeySequenceList(const QList<int> &sequence_list) {

  QList<QKeySequence> ret;
  ret.reserve(sequence_list.count());
  for (int sequence : sequence_list) {
    ret.append(sequence);
  }

  return ret;

}

void GlobalShortcutsBackendKGlobalAccel::GlobalShortcutPressed(const QString &component_unique, const QString &shortcut_unique, const qint64 timestamp) {

  Q_UNUSED(timestamp)

  if (QCoreApplication::applicationName() == component_unique && actions_.contains(shortcut_unique)) {
    const QList<QAction*> actions = actions_.values(shortcut_unique);
    for (QAction *action : actions) {
      qLog(Debug) << "Key" << action->shortcut() << "pressed.";
      if (action->isEnabled()) action->trigger();
    }
  }

}
