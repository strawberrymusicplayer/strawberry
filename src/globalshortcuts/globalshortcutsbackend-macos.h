/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef GLOBALSHORTCUTSBACKEND_MACOS_H
#define GLOBALSHORTCUTSBACKEND_MACOS_H

#include "config.h"

#include "globalshortcutsbackend.h"

#include <QObject>
#include <QMap>
#include <QAction>
#include <QKeySequence>

#include "includes/scoped_ptr.h"

class GlobalShortcutsBackendMacOSPrivate;

class GlobalShortcutsBackendMacOS : public GlobalShortcutsBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutsBackendMacOS(GlobalShortcutsManager *manager, QObject *parent = nullptr);
  virtual ~GlobalShortcutsBackendMacOS();

  bool IsAvailable() const override { return true; }

  static bool IsAccessibilityEnabled();
  static void ShowAccessibilityDialog();

  void MacMediaKeyPressed(const int key);

 protected:
  bool DoRegister() override;
  void DoUnregister() override;

 private:
  bool KeyPressed(const QKeySequence &sequence);

  QMap<QKeySequence, QAction*> shortcuts_;

  friend class GlobalShortcutsBackendMacOSPrivate;
  ScopedPtr<GlobalShortcutsBackendMacOSPrivate> p_;
};

#endif  // GLOBALSHORTCUTSBACKEND_MACOS_H
