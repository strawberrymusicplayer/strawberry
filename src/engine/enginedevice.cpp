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

using namespace Qt::Literals::StringLiterals;

EngineDevice::EngineDevice() : card(0), device(0) {}

QString EngineDevice::GuessIconName() const {

  if (description.contains("mcintosh"_L1, Qt::CaseInsensitive)) {
    return u"mcintosh"_s;
  }
  if (description.contains("electrocompaniet"_L1, Qt::CaseInsensitive)) {
    return u"electrocompaniet"_s;
  }
  if (description.contains("intel"_L1, Qt::CaseInsensitive)) {
    return u"intel"_s;
  }
  if (description.contains("realtek"_L1, Qt::CaseInsensitive)) {
    return u"realtek"_s;
  }
  if (description.contains("nvidia"_L1, Qt::CaseInsensitive)) {
    return u"nvidia"_s;
  }
  if (description.contains("headset"_L1, Qt::CaseInsensitive)) {
    return u"headset"_s;
  }
  if (description.contains("pulseaudio"_L1, Qt::CaseInsensitive)) {
    return u"pulseaudio"_s;
  }

  return u"soundcard"_s;

}
