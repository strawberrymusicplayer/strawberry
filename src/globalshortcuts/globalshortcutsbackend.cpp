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

#include "config.h"

#include <QObject>

#include "globalshortcutsbackend.h"
#include "globalshortcutsmanager.h"

GlobalShortcutsBackend::GlobalShortcutsBackend(GlobalShortcutsManager *manager, const Type type, QObject *parent)
    : QObject(parent),
      manager_(manager),
      type_(type),
      active_(false) {}

QString GlobalShortcutsBackend::name() const {

  switch (type_) {
    case Type::None:
      return QStringLiteral("None");
    case Type::KDE:
      return QStringLiteral("KDE");
    case Type::Gnome:
      return QStringLiteral("Gnome");
    case Type::Mate:
      return QStringLiteral("Mate");
    case Type::X11:
      return QStringLiteral("X11");
    case Type::macOS:
      return QStringLiteral("macOS");
    case Type::Win:
      return QStringLiteral("Windows");
  }

  return QString();

}

bool GlobalShortcutsBackend::Register() {

  bool ret = DoRegister();
  if (ret) active_ = true;
  return ret;

}

void GlobalShortcutsBackend::Unregister() {

  DoUnregister();
  active_ = false;

}
