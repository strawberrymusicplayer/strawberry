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

#include "config.h"

#include "globalshortcutsbackend-win.h"

#include "core/logging.h"

#include <QObject>
#include <QMap>
#include <QAction>
#include <QKeySequence>
#include <QtAlgorithms>

#include "globalshortcutsmanager.h"
#include "globalshortcutsbackend.h"
#include "globalshortcut.h"

GlobalShortcutsBackendWin::GlobalShortcutsBackendWin(GlobalShortcutsManager *manager, QObject *parent)
    : GlobalShortcutsBackend(manager, GlobalShortcutsBackend::Type::Win, parent),
      gshortcut_init_(nullptr) {}

GlobalShortcutsBackendWin::~GlobalShortcutsBackendWin() {
  GlobalShortcutsBackendWin::DoUnregister();
}

bool GlobalShortcutsBackendWin::IsAvailable() const {
  return true;
}

bool GlobalShortcutsBackendWin::DoRegister() {

  qLog(Debug) << "Registering";

  if (!gshortcut_init_) gshortcut_init_ = new GlobalShortcut(this);

  QList<GlobalShortcutsManager::Shortcut> shortcuts = manager_->shortcuts().values();
  for (const GlobalShortcutsManager::Shortcut &shortcut : shortcuts) {
    AddShortcut(shortcut.action);
  }

  return true;

}

bool GlobalShortcutsBackendWin::AddShortcut(QAction *action) {

  if (action->shortcut().isEmpty()) return false;

  GlobalShortcut *shortcut = new GlobalShortcut(action->shortcut(), this, this);
  QObject::connect(shortcut, &GlobalShortcut::activated, action, &QAction::trigger);
  shortcuts_ << shortcut;
  return true;

}

void GlobalShortcutsBackendWin::DoUnregister() {

  qLog(Debug) << "Unregistering";

  qDeleteAll(shortcuts_);
  shortcuts_.clear();

  if (gshortcut_init_) {
    delete gshortcut_init_;
    gshortcut_init_ = nullptr;
  }

}
