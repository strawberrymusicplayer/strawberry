/*
 * Strawberry Music Player
 * Copyright 2017-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>

#include "devicefinder.h"

DeviceFinder::DeviceFinder(const QString &name, const QStringList &outputs): name_(name), outputs_(outputs) {}

QString DeviceFinder::GuessIconName(const QString &description) {

  QString description_lower = description.toLower();

  if (description_lower.contains("mcintosh")) {
    return "mcintosh";
  }
  if (description_lower.contains("electrocompaniet")) {
    return "electrocompaniet";
  }
  if (description_lower.contains("intel")) {
    return "intel";
  }
  if (description_lower.contains("realtek")) {
    return "realtek";
  }
  if (description_lower.contains("nvidia")) {
    return "nvidia";
  }
  if (description_lower.contains("headset")) {
    return "headset";
  }
  if (description_lower.contains("pulseaudio")) {
    return "pulseaudio";
  }

  return "soundcard";

}
