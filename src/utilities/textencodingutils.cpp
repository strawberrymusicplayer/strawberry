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

#include <optional>

#include <uchardet.h>
#include <unicode/ucnv.h>

#include <QByteArray>
#include <QString>
#include <QStringConverter>
#include <QScopeGuard>

#include "textencodingutils.h"

namespace {

// Detects UTF-16 without a byte order mark from the null bytes, the ASCII characters have the null byte second in UTF-16LE and first in UTF-16BE.
std::optional<QStringConverter::Encoding> Utf16EncodingFromNullBytes(const QByteArray &data) {

  qsizetype null_bytes_even = 0;
  qsizetype null_bytes_odd = 0;
  for (qsizetype i = 0; i < data.length(); ++i) {
    if (data[i] == '\0') {
      if (i % 2 == 0) {
        ++null_bytes_even;
      }
      else {
        ++null_bytes_odd;
      }
    }
  }

  // Require null bytes for at least a quarter of the characters, so that a few stray null bytes in a text file in another encoding are not taken as UTF-16.
  if (null_bytes_odd * 8 >= data.length() && null_bytes_odd > null_bytes_even * 4) {
    return QStringConverter::Encoding::Utf16LE;
  }
  if (null_bytes_even * 8 >= data.length() && null_bytes_even > null_bytes_odd * 4) {
    return QStringConverter::Encoding::Utf16BE;
  }

  return std::nullopt;

}

}  // namespace

namespace Utilities {

QByteArray TextEncodingFromData(const QByteArray &data) {

  uchardet_t ucd = uchardet_new();
  if (!ucd) {
    return QByteArray();
  }
  const QScopeGuard scopeguard_ucd = qScopeGuard([ucd]() { uchardet_delete(ucd); });
  if (uchardet_handle_data(ucd, data.constData(), static_cast<size_t>(data.length())) != 0) {
    return QByteArray();
  }
  uchardet_data_end(ucd);

  return QByteArray(uchardet_get_charset(ucd));

}

QString TextFromData(const QByteArray &data) {

  // Unicode with a byte order mark.
  const std::optional<QStringConverter::Encoding> encoding = QStringConverter::encodingForData(data);
  if (encoding.has_value()) {
    QStringDecoder decoder(encoding.value());
    return decoder.decode(data);
  }

  // UTF-16 without a byte order mark, text with only characters below U+0080 in both bytes (for example Cyrillic in UTF-16LE) would pass as valid UTF-8, and uchardet detects it as ASCII.
  if (data.contains('\0')) {
    const std::optional<QStringConverter::Encoding> utf16_encoding = Utf16EncodingFromNullBytes(data);
    if (utf16_encoding.has_value()) {
      QStringDecoder decoder(utf16_encoding.value());
      return decoder.decode(data);
    }
  }

  QStringDecoder utf8_decoder(QStringConverter::Encoding::Utf8);
  const QString utf8_text = utf8_decoder.decode(data);
  if (!utf8_decoder.hasError()) {
    return utf8_text;
  }

  // Detect the encoding with uchardet, Qt only supports the Unicode encodings and Latin-1 on all platforms, so use ICU to convert other encodings.
  const QByteArray encoding_name = TextEncodingFromData(data);
  if (encoding_name.isEmpty()) {
    return utf8_text;
  }

  UErrorCode error_code = U_ZERO_ERROR;
  UConverter *converter = ucnv_open(encoding_name.constData(), &error_code);
  if (U_FAILURE(error_code)) {
    // ICU doesn't support all encodings uchardet detects (ISO-8859-16 and VISCII), Latin-1 keeps ISO-8859-16 text mostly readable.
    return QString::fromLatin1(data);
  }
  const QScopeGuard scopeguard_converter = qScopeGuard([converter]() { ucnv_close(converter); });

  const int32_t length = ucnv_toUChars(converter, nullptr, 0, data.constData(), static_cast<int32_t>(data.length()), &error_code);
  if (error_code != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error_code)) {
    return utf8_text;
  }
  error_code = U_ZERO_ERROR;
  QString text(length, Qt::Uninitialized);
  ucnv_toUChars(converter, reinterpret_cast<UChar*>(text.data()), length, data.constData(), static_cast<int32_t>(data.length()), &error_code);
  if (U_FAILURE(error_code)) {
    return utf8_text;
  }

  return text;

}

}  // namespace Utilities
