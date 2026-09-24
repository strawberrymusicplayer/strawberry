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

#include <cmath>

#include <QString>
#include <QColor>
#include <QPalette>

#include "colorutils.h"

namespace Utilities {

namespace {

// Colors below this relative luminance are dark.
// The point where a color has the same contrast ratio to black and white is 0.179, but that treats common accent colors like #0078d4 and #3584e4 as light, even if they are normally used with white text.
constexpr float kDarkRelativeLuminance = 0.25F;

// Converts a gamma encoded sRGB channel to linear light.
float LinearChannel(const float channel) {

  return channel <= 0.04045F ? channel / 12.92F : std::pow((channel + 0.055F) / 1.055F, 2.4F);

}

// Returns the relative luminance of the color as defined by WCAG, from 0 for black to 1 for white.
float RelativeLuminance(const QColor &color) {

  return 0.2126F * LinearChannel(color.redF()) + 0.7152F * LinearChannel(color.greenF()) + 0.0722F * LinearChannel(color.blueF());

}

}  // namespace

QString ColorToRgba(const QColor &c) {

  return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());

}

bool IsColorDark(const QColor &color) {

  return RelativeLuminance(color) < kDarkRelativeLuminance;

}

bool IsPaletteDark(const QPalette &palette, const QPalette::ColorRole background_role, const QPalette::ColorRole text_role) {

  return RelativeLuminance(palette.color(text_role)) > RelativeLuminance(palette.color(background_role));

}

float ColorDifference(const QColor &color1, const QColor &color2) {

  const float red = color1.redF() - color2.redF();
  const float green = color1.greenF() - color2.greenF();
  const float blue = color1.blueF() - color2.blueF();

  return std::sqrt((red * red + green * green + blue * blue) / 3.0F);

}

}  // namespace Utilities
