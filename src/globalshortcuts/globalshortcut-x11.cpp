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
#include <QMap>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QX11Info>
#include <QKeySequence>
#include <QFlags>

#include "globalshortcut.h"
#include "keymapper_x11.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

const QVector<quint32> GlobalShortcut::mask_modifiers_ = QVector<quint32>() << 0 << Mod2Mask << LockMask << (Mod2Mask | LockMask);

quint32 GlobalShortcut::nativeModifiers(Qt::KeyboardModifiers qt_mods) {

  quint32 native_mods = 0;
  if (qt_mods & Qt::ShiftModifier)    native_mods |= ShiftMask;
  if (qt_mods & Qt::ControlModifier)  native_mods |= ControlMask;
  if (qt_mods & Qt::AltModifier)      native_mods |= Mod1Mask;
  if (qt_mods & Qt::MetaModifier)     native_mods |= Mod4Mask;
  return native_mods;

}

quint32 GlobalShortcut::nativeKeycode(Qt::Key qt_key) {

  if (!QX11Info::display()) return 0;

  quint32 keysym = 0;
  if (KeyMapperX11::keymapper_x11_.contains(qt_key)) {
    keysym = KeyMapperX11::keymapper_x11_.value(qt_key);
  }
  else {
    keysym = XStringToKeysym(QKeySequence(qt_key).toString().toLatin1().data());
    if (keysym == NoSymbol) return 0;
  }
  return XKeysymToKeycode(QX11Info::display(), keysym);

}

bool GlobalShortcut::registerShortcut(quint32 native_key, quint32 native_mods) {
  if (!QX11Info::display()) return false;
  for (quint32 mask_mods : mask_modifiers_) {
    //xcb_grab_key(QX11Info::connection(), 1, QX11Info::appRootWindow(), (native_mods | mask_mods), native_key, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    XGrabKey(QX11Info::display(), native_key, (native_mods | mask_mods), QX11Info::appRootWindow(), True, GrabModeAsync, GrabModeAsync);
  }
  return true;
}

bool GlobalShortcut::unregisterShortcut(quint32 native_key, quint32 native_mods) {
  if (!QX11Info::display()) return false;
  for (quint32 mask_mods : mask_modifiers_) {
    XUngrabKey(QX11Info::display(), native_key, native_mods | mask_mods, QX11Info::appRootWindow());
  }
  return true;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, qintptr *result) {
#else
bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, long *result) {
#endif

  Q_UNUSED(eventtype);
  Q_UNUSED(result);

  xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
  if ((event->response_type & 127) != XCB_KEY_PRESS) return false;

  xcb_key_press_event_t *key_press_event = static_cast<xcb_key_press_event_t *>(message);
  if (!key_press_event) return false;

  quint32 keycode = key_press_event->detail;
  unsigned int keystate = 0;
  if (key_press_event->state & XCB_MOD_MASK_1) keystate |= Mod1Mask;
  if (key_press_event->state & XCB_MOD_MASK_CONTROL) keystate |= ControlMask;
  if (key_press_event->state & XCB_MOD_MASK_4) keystate |= Mod4Mask;
  if (key_press_event->state & XCB_MOD_MASK_SHIFT) keystate |= ShiftMask;
  activateShortcut(keycode, keystate & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask));

  return false;

}
