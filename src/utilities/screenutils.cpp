/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QGuiApplication>
#include <QWidget>
#include <QScreen>
#include <QWindow>

#include "screenutils.h"

namespace Utilities {

QScreen *GetScreen(QWidget *widget) {

  return widget->screen();

}

void CenterWidgetOnScreen(QScreen *screen, QWidget *widget) {

  if (screen) {
    const QRect sr = screen->availableGeometry();
    const QRect wr({}, widget->size().boundedTo(sr.size()));
    widget->resize(wr.size());
    widget->move(sr.center() - wr.center());
  }

}

}  // namespace Utilities
