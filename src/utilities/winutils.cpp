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

#include <windows.h>
#include <dwmapi.h>

#include <QWindow>
#include <QRegion>

#include "winutils.h"

namespace Utilities {

HRGN qt_RectToHRGN(const QRect &rc);
HRGN qt_RectToHRGN(const QRect &rc) {
  return CreateRectRgn(rc.left(), rc.top(), rc.right() + 1, rc.bottom() + 1);
}

void enableBlurBehindWindow(QWindow *window, const QRegion &region) {

  DWM_BLURBEHIND dwmbb = { 0, 0, nullptr, 0 };
  dwmbb.dwFlags = DWM_BB_ENABLE;
  dwmbb.fEnable = TRUE;
  HRGN rgn = nullptr;
  if (!region.isNull()) {
    rgn = region.toHRGN();
    if (rgn) {
      dwmbb.hRgnBlur = rgn;
      dwmbb.dwFlags |= DWM_BB_BLURREGION;
    }
  }
  DwmEnableBlurBehindWindow(reinterpret_cast<HWND>(window->winId()), &dwmbb);
  if (rgn) {
    DeleteObject(rgn);
  }

}

}  // namespace Utilities
