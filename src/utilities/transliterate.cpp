/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstdio>
#include <string>

#include <unicode/translit.h>

#include <QByteArray>
#include <QString>

#include "includes/scoped_ptr.h"
#include "transliterate.h"

namespace Utilities {

QString Transliterate(const QString &accented_str) {

  UErrorCode errorcode = U_ZERO_ERROR;
  ScopedPtr<icu::Transliterator> transliterator;
  transliterator.reset(icu::Transliterator::createInstance("Any-Latin; Latin-ASCII;", UTRANS_FORWARD, errorcode));

  if (!transliterator) return accented_str;

  QByteArray accented_data = accented_str.toUtf8();
  icu::UnicodeString ustring = icu::UnicodeString(accented_data.constData());
  transliterator->transliterate(ustring);

  std::string unaccented_str;
  ustring.toUTF8String(unaccented_str);

  return QString::fromStdString(unaccented_str);

}  // Transliterate

}  // namespace Utilities
