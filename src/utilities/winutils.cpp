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

HRGN toHRGN(const QRegion &region);
HRGN toHRGN(const QRegion &region) {

#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return region.toHRGN();
#  else

  const int rect_count = region.rectCount();
  if (rect_count == 0) {
    return nullptr;
  }

  HRGN resultRgn = nullptr;
  QRegion::const_iterator rects = region.begin();
  resultRgn = qt_RectToHRGN(rects[0]);
  for (int i = 1; i < rect_count; ++i) {
    HRGN tmpRgn = qt_RectToHRGN(rects[i]);
    const int res = CombineRgn(resultRgn, resultRgn, tmpRgn, RGN_OR);
    if (res == ERROR) qWarning("Error combining HRGNs.");
    DeleteObject(tmpRgn);
  }

  return resultRgn;

#  endif  // Qt 6
}

void enableBlurBehindWindow(QWindow *window, const QRegion &region) {

  DWM_BLURBEHIND dwmbb = { 0, 0, nullptr, 0 };
  dwmbb.dwFlags = DWM_BB_ENABLE;
  dwmbb.fEnable = TRUE;
  HRGN rgn = nullptr;
  if (!region.isNull()) {
    rgn = toHRGN(region);
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
