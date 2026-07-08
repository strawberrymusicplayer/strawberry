/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>

#include "styleutils.h"

using namespace Qt::StringLiterals;

namespace Utilities {

bool StyleHasCustomPaletteColorsSupport(const QString &style_name) {

#if defined(Q_OS_WIN32)
  return style_name.compare(u"windows"_s, Qt::CaseInsensitive) != 0 && style_name.compare(u"windowsvista"_s, Qt::CaseInsensitive) != 0 && style_name.compare(u"windows11"_s, Qt::CaseInsensitive) != 0;
#elif defined(Q_OS_MACOS)
  return style_name.compare(u"macos"_s, Qt::CaseInsensitive) != 0;
#else
  return style_name.compare(u"breeze"_s, Qt::CaseInsensitive) != 0;
#endif

}

bool StyleHasDarkModeSupport(const QString &style_name) {

#if defined(Q_OS_WIN32)
  return style_name.compare(u"windows"_s, Qt::CaseInsensitive) == 0 || style_name.compare(u"windows11"_s, Qt::CaseInsensitive) == 0;
#elif defined(Q_OS_MACOS)
  return style_name.compare(u"macos"_s, Qt::CaseInsensitive) == 0;
#else
  Q_UNUSED(style_name)
  return false;
#endif

}

}  // namespace Utilities
