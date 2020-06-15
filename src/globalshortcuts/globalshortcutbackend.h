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

#ifndef GLOBALSHORTCUTBACKEND_H
#define GLOBALSHORTCUTBACKEND_H

#include "config.h"

#include <QObject>
#include <QString>

class GlobalShortcuts;

class GlobalShortcutBackend : public QObject {
  Q_OBJECT

 public:
  explicit GlobalShortcutBackend(GlobalShortcuts *parent = nullptr);

  bool is_active() const { return active_; }

  bool Register();
  void Unregister();

 signals:
  void RegisterFinished(bool success);

 protected:
  virtual bool DoRegister() = 0;
  virtual void DoUnregister() = 0;

  GlobalShortcuts *manager_;
  bool active_;

};

#endif  // GLOBALSHORTCUTBACKEND_H
