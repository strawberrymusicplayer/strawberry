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

#include <QtGlobal>
#include <QByteArray>

#include <qt_windows.h>

#include "globalshortcut.h"
#include "keymapper_win.h"

#include "core/logging.h"

int GlobalShortcut::nativeModifiers(const Qt::KeyboardModifiers qt_mods) {

  int native_mods = 0;
  if (qt_mods & Qt::ShiftModifier) native_mods |= MOD_SHIFT;
  if (qt_mods & Qt::ControlModifier) native_mods |= MOD_CONTROL;
  if (qt_mods & Qt::AltModifier) native_mods |= MOD_ALT;
  if (qt_mods & Qt::MetaModifier) native_mods |= MOD_WIN;
  return native_mods;

}

int GlobalShortcut::nativeKeycode(const Qt::Key qt_keycode) {

  int key_code = 0;
  if (KeyMapperWin::keymapper_win_.contains(qt_keycode)) {
    key_code = static_cast<int>(KeyMapperWin::keymapper_win_.value(qt_keycode));
  }
  return key_code;

}

int GlobalShortcut::nativeKeycode2(const Qt::Key qt_keycode) {

  switch (qt_keycode) {
    case Qt::Key_0:
      return VK_NUMPAD0;
    case Qt::Key_1:
      return VK_NUMPAD1;
    case Qt::Key_2:
      return VK_NUMPAD2;
    case Qt::Key_3:
      return VK_NUMPAD3;
    case Qt::Key_4:
      return VK_NUMPAD4;
    case Qt::Key_5:
      return VK_NUMPAD5;
    case Qt::Key_6:
      return VK_NUMPAD6;
    case Qt::Key_7:
      return VK_NUMPAD7;
    case Qt::Key_8:
      return VK_NUMPAD8;
    case Qt::Key_9:
      return VK_NUMPAD9;
    default:
      break;
  }

  return 0;

}

bool GlobalShortcut::registerShortcut(const int native_key, const int native_mods) {

  return RegisterHotKey(0, native_mods ^ native_key, static_cast<UINT>(native_mods), static_cast<UINT>(native_key));

}

bool GlobalShortcut::unregisterShortcut(const int native_key, const int native_mods) {

  return UnregisterHotKey(0, native_mods ^ native_key);

}

bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, qintptr *result) {

  Q_UNUSED(eventtype);
  Q_UNUSED(result);

  MSG *msg = static_cast<MSG*>(message);
  if (msg->message != WM_HOTKEY) return false;

  quint32 key_code = HIWORD(msg->lParam);
  quint32 modifiers = LOWORD(msg->lParam);
  activateShortcut(key_code, modifiers);

  return true;

}
