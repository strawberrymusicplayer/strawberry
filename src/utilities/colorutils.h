/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLORUTILS_H
#define COLORUTILS_H

#include <QString>
#include <QColor>
#include <QPalette>

namespace Utilities {

QString ColorToRgba(const QColor &color);

// Returns true if the color is dark, based on its relative luminance.
bool IsColorDark(const QColor &color);

// Returns true if the palette is dark, meaning the text has a higher relative luminance than the background.
bool IsPaletteDark(const QPalette &palette, const QPalette::ColorRole background_role = QPalette::Window, const QPalette::ColorRole text_role = QPalette::WindowText);

// Returns the difference between two colors, from 0 for the same color to 1 for black and white.
// For grays this is the lightness difference, but it also includes differences in hue and saturation.
float ColorDifference(const QColor &color1, const QColor &color2);

}  // namespace Utilities

#endif  // COLORUTILS_H
