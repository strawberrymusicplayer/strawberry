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

#include <QString>
#include <QColor>

#include "colorutils.h"

namespace Utilities {

QString ColorToRgba(const QColor &c) {

  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(c.alpha());

}

bool IsColorDark(const QColor &color) {
  return ((30 * color.red() + 59 * color.green() + 11 * color.blue()) / 100) <= 130;
}

}  // namespace Utilities
