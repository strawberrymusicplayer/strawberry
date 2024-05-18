/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <unicode/ucsdet.h>

#include <QByteArray>
#include <QString>
#include <QScopeGuard>

#include "textencodingutils.h"

namespace Utilities {

QByteArray TextEncodingFromData(const QByteArray &data) {

  UErrorCode error_code = U_ZERO_ERROR;
  UCharsetDetector *csd = ucsdet_open(&error_code);
  if (error_code != U_ZERO_ERROR) {
    return QByteArray();
  }
  const QScopeGuard scopeguard_csd = qScopeGuard([csd]() { ucsdet_close(csd); });
  ucsdet_setText(csd, data.constData(), static_cast<int>(data.length()), &error_code);
  if (error_code != U_ZERO_ERROR) {
    return QByteArray();
  }
  const UCharsetMatch *ucm = ucsdet_detect(csd, &error_code);
  if (error_code != U_ZERO_ERROR) {
    return QByteArray();
  }
  const char *encoding_name = ucsdet_getName(ucm, &error_code);
  if (error_code != U_ZERO_ERROR) {
    return QByteArray();
  }

  return encoding_name;

}

}  // namespace Utilities
