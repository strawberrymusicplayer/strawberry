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

  if (description.contains("mcintosh", Qt::CaseInsensitive)) {
    return "mcintosh";
  }
  if (description.contains("electrocompaniet", Qt::CaseInsensitive)) {
    return "electrocompaniet";
  }
  if (description.contains("intel", Qt::CaseInsensitive)) {
    return "intel";
  }
  if (description.contains("realtek", Qt::CaseInsensitive)) {
    return "realtek";
  }
  if (description.contains("nvidia", Qt::CaseInsensitive)) {
    return "nvidia";
  }
  if (description.contains("headset", Qt::CaseInsensitive)) {
    return "headset";
  }
  if (description.contains("pulseaudio", Qt::CaseInsensitive)) {
    return "pulseaudio";
  }

  return "soundcard";

}
