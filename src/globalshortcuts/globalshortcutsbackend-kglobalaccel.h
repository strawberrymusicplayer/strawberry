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

#ifndef GLOBALSHORTCUTSBACKEND_KDE_H
#define GLOBALSHORTCUTSBACKEND_KDE_H

#include "config.h"

#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>
#include <QKeySequence>

#include "globalshortcutsbackend.h"
#include "globalshortcutsmanager.h"

class QDBusPendingCallWatcher;
class QAction;

class OrgKdeKGlobalAccelInterface;
class OrgKdeKglobalaccelComponentInterface;

class GlobalShortcutsBackendKGlobalAccel : public GlobalShortcutsBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutsBackendKGlobalAccel(GlobalShortcutsManager *manager, QObject *parent = nullptr);

  bool IsAvailable() const override;
  static bool IsKGlobalAccelAvailable();

 protected:
  bool DoRegister() override;
  void DoUnregister() override;

 private:
  bool IsMediaShortcut(const GlobalShortcutsManager::Shortcut &shortcut) const;
  bool RegisterShortcut(const GlobalShortcutsManager::Shortcut &shortcut);
  static QStringList GetActionId(const QString &id, const QAction *action);
  static QList<int> ToIntList(const QList<QKeySequence> &sequence_list);
  static QList<QKeySequence> ToKeySequenceList(const QList<int> &sequence_list);

 private Q_SLOTS:
  void RegisterFinished(QDBusPendingCallWatcher *watcher);
  void GlobalShortcutPressed(const QString &component_unique, const QString &shortcut_unique, const qint64 timestamp);

 private:
  OrgKdeKGlobalAccelInterface *interface_;
  OrgKdeKglobalaccelComponentInterface *component_;
  QMultiHash<QString, QAction*> actions_;
};

#endif  // GLOBALSHORTCUTSBACKEND_KDE_H
