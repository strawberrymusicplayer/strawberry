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

#include <QApplication>
#include <QGuiApplication>
#include <QList>
#include <QByteArray>
#include <QKeySequence>
#include <QFlags>
#include <QScreen>

#include "globalshortcut.h"
#include "keymapper_x11.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

const QList<quint32> GlobalShortcut::mask_modifiers_ = QList<quint32>() << 0 << Mod2Mask << LockMask << (Mod2Mask | LockMask);

namespace {

Display *X11Display() {

  if (!qApp) return nullptr;

  if (QNativeInterface::QX11Application *x11_app = qApp->nativeInterface<QNativeInterface::QX11Application>()) {
    return x11_app->display();
  }
  return nullptr;

}

quint32 AppRootWindow() {

  if (QNativeInterface::QX11Application *x11_app = qApp->nativeInterface<QNativeInterface::QX11Application>()) {
    if (x11_app->display()) {
      return XDefaultRootWindow(x11_app->display());
    }
  }
  return 0;

}

}  // namespace

int GlobalShortcut::nativeModifiers(const Qt::KeyboardModifiers qt_mods) {

  int native_mods = 0;
  if (qt_mods & Qt::ShiftModifier)    native_mods |= ShiftMask;
  if (qt_mods & Qt::ControlModifier)  native_mods |= ControlMask;
  if (qt_mods & Qt::AltModifier)      native_mods |= Mod1Mask;
  if (qt_mods & Qt::MetaModifier)     native_mods |= Mod4Mask;
  return native_mods;

}

int GlobalShortcut::nativeKeycode(const Qt::Key qt_keycode) {

  Display *disp = X11Display();
  if (!disp) return false;

  quint32 keysym = 0;
  if (KeyMapperX11::keymapper_x11_.contains(qt_keycode)) {
    keysym = KeyMapperX11::keymapper_x11_.value(qt_keycode);
  }
  else {
    keysym = XStringToKeysym(QKeySequence(qt_keycode).toString().toLatin1().data());
    if (keysym == NoSymbol) return 0;
  }
  return XKeysymToKeycode(disp, keysym);

}

int GlobalShortcut::nativeKeycode2(const Qt::Key qt_keycode) {
  Q_UNUSED(qt_keycode)
  return 0;
}

bool GlobalShortcut::registerShortcut(const int native_key, const int native_mods) {

  Display *disp = X11Display();
  if (!disp) return false;

  for (const quint32 mask_mods : mask_modifiers_) {
    XGrabKey(disp, native_key, (native_mods | mask_mods), AppRootWindow(), True, GrabModeAsync, GrabModeAsync);
  }
  return true;

}

bool GlobalShortcut::unregisterShortcut(const int native_key, const int native_mods) {

  Display *disp = X11Display();
  if (!disp) return false;

  for (const quint32 mask_mods : mask_modifiers_) {
    XUngrabKey(disp, native_key, native_mods | mask_mods, AppRootWindow());
  }
  return true;

}

bool GlobalShortcut::nativeEventFilter(const QByteArray &eventtype, void *message, qintptr *result) {

  Q_UNUSED(eventtype);
  Q_UNUSED(result);

  xcb_generic_event_t *event = static_cast<xcb_generic_event_t*>(message);
  if ((event->response_type & 127) != XCB_KEY_PRESS) return false;

  xcb_key_press_event_t *key_press_event = static_cast<xcb_key_press_event_t*>(message);
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
