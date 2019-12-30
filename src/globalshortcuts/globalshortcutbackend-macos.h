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

#ifndef GLOBALSHORTCUTBACKEND_MACOS_H
#define GLOBALSHORTCUTBACKEND_MACOS_H

#include "config.h"


#include <memory>

#include "globalshortcutbackend.h"

#include <QObject>
#include <QMap>
#include <QAction>
#include <QKeySequence>

class GlobalShortcut;

class GlobalShortcutBackendMacOSPrivate;

class GlobalShortcutBackendMacOS : public GlobalShortcutBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutBackendMacOS(GlobalShortcuts* parent);
  virtual ~GlobalShortcutBackendMacOS();

  bool IsAccessibilityEnabled() const;
  void ShowAccessibilityDialog();

  void MacMediaKeyPressed(int key);

 protected:
  bool DoRegister();
  void DoUnregister();

 private:
  bool KeyPressed(const QKeySequence &sequence);

  QMap<QKeySequence, QAction*> shortcuts_;

  friend class GlobalShortcutBackendMacOSPrivate;
  std::unique_ptr<GlobalShortcutBackendMacOSPrivate> p_;
};

#endif  // GLOBALSHORTCUTBACKEND_MACOS_H

