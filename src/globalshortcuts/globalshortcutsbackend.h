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

#ifndef GLOBALSHORTCUTSBACKEND_H
#define GLOBALSHORTCUTSBACKEND_H

#include "config.h"

#include <QObject>
#include <QString>

class GlobalShortcutsManager;

class GlobalShortcutsBackend : public QObject {
  Q_OBJECT

 public:
  enum class Type {
    None = 0,
    KGlobalAccel,
    X11,
    macOS,
    Win
  };

  explicit GlobalShortcutsBackend(GlobalShortcutsManager *manager, const Type type, QObject *parent = nullptr);

  Type type() const { return type_; }
  QString name() const;

  virtual bool IsAvailable() const = 0;

  bool Register();
  void Unregister();

  bool is_active() const { return active_; }

 Q_SIGNALS:
  void RegisterFinished(const bool success);

 protected:
  virtual bool DoRegister() = 0;
  virtual void DoUnregister() = 0;

  GlobalShortcutsManager *manager_;
  Type type_;
  bool active_;
};

#endif  // GLOBALSHORTCUTSBACKEND_H
