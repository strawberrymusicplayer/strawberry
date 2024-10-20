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

#include <QString>
#include <QChar>

#include <QRandomGenerator>

#include "randutils.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

QString GetRandomStringWithChars(const int len) {
  const QString UseCharacters(u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"_s);
  return GetRandomString(len, UseCharacters);
}

QString GetRandomStringWithCharsAndNumbers(const int len) {
  const QString UseCharacters(u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"_s);
  return GetRandomString(len, UseCharacters);
}

QString CryptographicRandomString(const int len) {
  const QString UseCharacters(u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"_s);
  return GetRandomString(len, UseCharacters);
}

QString GetRandomString(const int len, const QString &UseCharacters) {

  QString randstr;
  for (int i = 0; i < len; ++i) {
    const qint64 index = QRandomGenerator::global()->bounded(0, UseCharacters.length());
    QChar nextchar = UseCharacters.at(index);
    randstr.append(nextchar);
  }

  return randstr;

}

}  // namespace Utilities
