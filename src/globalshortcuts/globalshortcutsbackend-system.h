/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GLOBALSHORTCUTSBACKEND_SYSTEM_H
#define GLOBALSHORTCUTSBACKEND_SYSTEM_H

#include "config.h"

#include "core/logging.h"

#include <QObject>
#include <QList>
#include <QString>

#include "globalshortcutsbackend.h"

class QAction;
class GlobalShortcutsManager;
class GlobalShortcut;

class GlobalShortcutsBackendSystem : public GlobalShortcutsBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutsBackendSystem(GlobalShortcutsManager *manager, QObject *parent = nullptr);
  ~GlobalShortcutsBackendSystem() override;

  bool IsAvailable() override { return true; }

 protected:
  bool DoRegister() override;
  void DoUnregister() override;

 private:
  bool AddShortcut(QAction *action);
  bool RemoveShortcut(QAction *action);

  QList<GlobalShortcut*> shortcuts_;
  GlobalShortcut *gshortcut_init_;

};

#endif  // GLOBALSHORTCUTSBACKEND_SYSTEM_H
