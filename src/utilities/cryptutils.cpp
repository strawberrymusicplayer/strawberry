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

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

#include "cryptutils.h"

namespace Utilities {

QByteArray Hmac(const QByteArray &key, const QByteArray &data, const QCryptographicHash::Algorithm method) {

  constexpr int block_size = 64;
  Q_ASSERT(key.length() <= block_size);

  QByteArray inner_padding(block_size, static_cast<char>(0x36));
  QByteArray outer_padding(block_size, static_cast<char>(0x5c));

  for (int i = 0; i < key.length(); ++i) {
    inner_padding[i] = static_cast<char>(inner_padding[i] ^ key[i]);
    outer_padding[i] = static_cast<char>(outer_padding[i] ^ key[i]);
  }

  QByteArray part;
  part.append(inner_padding);
  part.append(data);

  QByteArray total;
  total.append(outer_padding);
  total.append(QCryptographicHash::hash(part, method));

  return QCryptographicHash::hash(total, method);

}

QByteArray HmacSha256(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Sha256);
}

QByteArray HmacMd5(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Md5);
}

QByteArray HmacSha1(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Sha1);
}

}  // namespace Utilities
