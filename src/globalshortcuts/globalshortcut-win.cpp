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

#include <QtGlobal>
#include <QByteArray>

#include <qt_windows.h>

#include "globalshortcut.h"
#include "keymapper_win.h"

quint32 GlobalShortcut::nativeModifiers(Qt::KeyboardModifiers qt_mods) {

  quint32 native_mods = 0;
  if (qt_mods & Qt::ShiftModifier) native_mods |= MOD_SHIFT;
  if (qt_mods & Qt::ControlModifier) native_mods |= MOD_CONTROL;
  if (qt_mods & Qt::AltModifier) native_mods |= MOD_ALT;
  if (qt_mods & Qt::MetaModifier) native_mods |= MOD_WIN;
  return native_mods;

}

quint32 GlobalShortcut::nativeKeycode(Qt::Key qt_key) {

  quint32 key_code = 0;
  if (KeyMapperWin::keymapper_win_.contains(qt_key)) {
    key_code = KeyMapperWin::keymapper_win_.value(qt_key);
  }
  return key_code;

}

bool GlobalShortcut::registerShortcut(quint32 native_key, quint32 native_mods) {
  return RegisterHotKey(0, native_mods ^ native_key, native_mods, native_key);
}

bool GlobalShortcut::unregisterShortcut(quint32 native_key, quint32 native_mods) {
  return UnregisterHotKey(0, native_mods ^ native_key);
}


#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, qintptr *result) {
#else
bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, long *result) {
#endif

  Q_UNUSED(eventtype);
  Q_UNUSED(result);

  MSG *msg = static_cast<MSG*>(message);
  if (msg->message != WM_HOTKEY) return false;

  quint32 key_code = HIWORD(msg->lParam);
  quint32 modifiers = LOWORD(msg->lParam);
  activateShortcut(key_code, modifiers);
  return true;

}
