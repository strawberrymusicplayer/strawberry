/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QAbstractEventDispatcher>
#include <QPair>
#include <QHash>
#include <QFlags>
#include <QtDebug>

#include "core/logging.h"

#include "globalshortcut.h"

GlobalShortcut *GlobalShortcut::initialized_ = nullptr;
QHash<QPair<quint32, quint32>, GlobalShortcut*> GlobalShortcut::internal_shortcuts_;

GlobalShortcut::GlobalShortcut(QObject *parent) : QObject(parent),
  qt_key_(Qt::Key(0)),
  qt_mods_(Qt::NoModifier),
  native_key_(0),
  native_mods_(0) {

  Q_ASSERT(!initialized_);

  QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
  initialized_ = this;

}

GlobalShortcut::GlobalShortcut(QKeySequence shortcut, GlobalShortcutBackend *backend, QObject *parent) : QObject(parent),
  backend_(backend),
  shortcut_(shortcut),
  qt_key_(Qt::Key(0)),
  qt_mods_(Qt::NoModifier),
  native_key_(0),
  native_mods_(0) {

  Q_ASSERT(initialized_);
  setShortcut(shortcut);

}

GlobalShortcut::~GlobalShortcut() {

  if (this == initialized_) {
    QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);
    initialized_ = nullptr;
  }
  else {
    unsetShortcut();
  }

}

bool GlobalShortcut::setShortcut(const QKeySequence &shortcut) {

  Q_ASSERT(initialized_);

  if (shortcut.isEmpty()) return false;
  shortcut_ = shortcut;

  Qt::KeyboardModifiers all_mods = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
  qt_key_ = Qt::Key((shortcut[0] ^ all_mods) & shortcut[0]);
  qt_mods_ = Qt::KeyboardModifiers(shortcut[0] & all_mods);

  native_key_ = nativeKeycode(qt_key_);
  if (native_key_ == 0) return false;
  native_mods_ = nativeModifiers(qt_mods_);

  bool result = registerShortcut(native_key_, native_mods_);
  if (result) {
    internal_shortcuts_.insert(qMakePair(native_key_, native_mods_), this);
    qLog(Info) << "Registered shortcut" << shortcut_.toString();
  }
  else {
    qLog(Error) << "Failed to register shortcut" << shortcut_.toString();
  }

  return result;

}

bool GlobalShortcut::unsetShortcut() {

  Q_ASSERT(initialized_);

  QPair<quint32, quint32> hash = qMakePair(native_key_, native_mods_);
  if (internal_shortcuts_.contains(hash)) {
    GlobalShortcut *gshortcut = internal_shortcuts_.value(hash);
    if (gshortcut != this) return false;
  }

  bool result = unregisterShortcut(native_key_, native_mods_);
  if (result) {
    if (internal_shortcuts_.contains(hash)) {
      internal_shortcuts_.remove(hash);
    }
    qLog(Info) << "Unregister shortcut" << shortcut_.toString();
  }
  else {
    qLog(Error) << "Failed to unregister shortcut" << shortcut_.toString();
  }

  qt_key_ = Qt::Key(0);
  qt_mods_ = Qt::KeyboardModifiers(0);
  native_key_ = 0;
  native_mods_ = 0;

  return result;

}

void GlobalShortcut::activateShortcut(quint32 native_key, quint32 native_mod) {

  Q_ASSERT(initialized_);

  QPair<quint32, quint32> hash = qMakePair(native_key, native_mod);
  if (!internal_shortcuts_.contains(hash)) return;

  GlobalShortcut *gshortcut = internal_shortcuts_.value(hash);
  if (gshortcut && gshortcut != initialized_) {
    emit gshortcut->activated();
  }

}
