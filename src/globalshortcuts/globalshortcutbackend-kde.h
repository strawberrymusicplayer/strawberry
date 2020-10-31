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

#ifndef GLOBALSHORTCUTBACKEND_KDE_H
#define GLOBALSHORTCUTBACKEND_KDE_H

#include "config.h"

#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>
#include <QKeySequence>

#include "globalshortcutbackend.h"
#include "globalshortcuts.h"

class QDBusPendingCallWatcher;
class QAction;

class OrgKdeKGlobalAccelInterface;
class OrgKdeKglobalaccelComponentInterface;

class GlobalShortcutBackendKDE : public GlobalShortcutBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutBackendKDE(GlobalShortcuts *parent);

  static const char *kKdeService;

 protected:
  bool DoRegister() override;
  void DoUnregister() override;

 private:
  bool RegisterShortcut(const GlobalShortcuts::Shortcut &shortcut);
  static QStringList GetActionId(const QString &id, const QAction *action);
  static QList<int> ToIntList(const QList<QKeySequence> &sequence_list);
  static QList<QKeySequence> ToKeySequenceList(const QList<int> &sequence_list);

 private slots:
  void RegisterFinished(QDBusPendingCallWatcher *watcher);
  void GlobalShortcutPressed(const QString &component_unique, const QString &shortcut_unique, qlonglong);

 private:
  static const char *kKdePath;

  OrgKdeKGlobalAccelInterface *interface_;
  OrgKdeKglobalaccelComponentInterface *component_;
  QMultiHash<QString, QAction*> actions_;
};

#endif  // GLOBALSHORTCUTBACKEND_KDE_H
