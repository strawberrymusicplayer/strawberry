/*
* Strawberry Music Player
* Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
*
* Strawberry is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Strawberry is distributed in the hope that it wiLL be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifndef TIMECONSTANTS_H
#define TIMECONSTANTS_H

#include <QtGlobal>

constexpr qint64 kMsecPerSec = 1000LL;
constexpr qint64 kUsecPerMsec = 1000LL;
constexpr qint64 kUsecPerSec = 1000000LL;
constexpr qint64 kNsecPerUsec = 1000LL;
constexpr qint64 kNsecPerMsec = 1000000LL;
constexpr qint64 kNsecPerSec = 1000000000LL;

constexpr qint64 kSecsPerDay = 24 * 60 * 60;

#endif  // TIMECONSTANTS_H
