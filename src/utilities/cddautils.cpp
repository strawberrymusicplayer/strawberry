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

#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "cddautils.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

QUrl CDDAUrlFromDevice(const QString &device) {

  // Windows drives from libcdio are like \\.\D:, libcdio also accepts the drive as D:.
  static const QRegularExpression regex_windows_drive(u"^(?:\\\\\\\\\\.\\\\)?([A-Za-z]:)$"_s);
  const QRegularExpressionMatch match = regex_windows_drive.match(device);
  if (match.hasMatch()) {
    return QUrl(u"cdda:///"_s + match.captured(1));
  }

  return QUrl(u"cdda://"_s + device);

}

QString CDDADeviceFromUrlPath(const QString &path) {

  // A Windows drive is the path without the leading slash, like /D:.
  static const QRegularExpression regex_windows_drive(u"^/([A-Za-z]:)$"_s);
  const QRegularExpressionMatch match = regex_windows_drive.match(path);
  if (match.hasMatch()) {
    return match.captured(1);
  }

  return path;

}

}  // namespace Utilities
