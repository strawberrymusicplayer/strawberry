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

#ifndef CDDAUTILS_H
#define CDDAUTILS_H

#include <QString>
#include <QUrl>

namespace Utilities {

// Returns the cdda URL of a CD device, the device is the path of the URL.
// Windows devices, like \\.\D: from libcdio, would be the host of the URL, which can't contain them, so the drive is put in the path as /D:.
QUrl CDDAUrlFromDevice(const QString &device);

// Returns the CD device from the path of a cdda URL, the path of a track URL without the track number.
QString CDDADeviceFromUrlPath(const QString &path);

}  // namespace Utilities

#endif  // CDDAUTILS_H
