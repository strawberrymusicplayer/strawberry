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

#ifdef HAVE_ICU
#  include <unicode/translit.h>
#else
#  include <iconv.h>
#endif

#include <QByteArray>
#include <QString>

#include "core/scoped_ptr.h"
#include "transliterate.h"

namespace Utilities {

QString Transliterate(const QString &accented_str) {

#ifdef HAVE_ICU

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

#else

#ifdef LC_ALL
  setlocale(LC_ALL, "");
#endif

  iconv_t conv = iconv_open("ASCII//TRANSLIT", "UTF-8");
  if (conv == reinterpret_cast<iconv_t>(-1)) return accented_str;

  QByteArray utf8 = accented_str.toUtf8();

  size_t input_len = utf8.length() + 1;
  char *input_ptr = new char[input_len];
  char *input = input_ptr;

  size_t output_len = input_len * 2;
  char *output_ptr = new char[output_len];
  char *output = output_ptr;

  snprintf(input, input_len, "%s", utf8.constData());

  iconv(conv, &input, &input_len, &output, &output_len);
  iconv_close(conv);

  QString ret(output_ptr);
  ret = ret.replace('?', '_');

  delete[] input_ptr;
  delete[] output_ptr;

  return ret;

#endif  // HAVE_ICU

}  // Transliterate

}  // namespace Utilities
