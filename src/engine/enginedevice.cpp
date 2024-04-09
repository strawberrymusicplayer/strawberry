/*
* Strawberry Music Player
* Copyright 2021-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include "enginedevice.h"

EngineDevice::EngineDevice() : card(0), device(0) {}

QString EngineDevice::GuessIconName() const {

  if (description.contains(QLatin1String("mcintosh"), Qt::CaseInsensitive)) {
    return QStringLiteral("mcintosh");
  }
  if (description.contains(QLatin1String("electrocompaniet"), Qt::CaseInsensitive)) {
    return QStringLiteral("electrocompaniet");
  }
  if (description.contains(QLatin1String("intel"), Qt::CaseInsensitive)) {
    return QStringLiteral("intel");
  }
  if (description.contains(QLatin1String("realtek"), Qt::CaseInsensitive)) {
    return QStringLiteral("realtek");
  }
  if (description.contains(QLatin1String("nvidia"), Qt::CaseInsensitive)) {
    return QStringLiteral("nvidia");
  }
  if (description.contains(QLatin1String("headset"), Qt::CaseInsensitive)) {
    return QStringLiteral("headset");
  }
  if (description.contains(QLatin1String("pulseaudio"), Qt::CaseInsensitive)) {
    return QStringLiteral("pulseaudio");
  }

  return QStringLiteral("soundcard");

}
