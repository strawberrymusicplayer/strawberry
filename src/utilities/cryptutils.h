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

#ifndef CRYPTUTILS_H
#define CRYPTUTILS_H

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

namespace Utilities {

QByteArray Hmac(const QByteArray &key, const QByteArray &data, const QCryptographicHash::Algorithm method);
QByteArray HmacMd5(const QByteArray &key, const QByteArray &data);
QByteArray HmacSha256(const QByteArray &key, const QByteArray &data);
QByteArray HmacSha1(const QByteArray &key, const QByteArray &data);

}  // namespace Utilities

#endif  // CRYPTUTILS_H
