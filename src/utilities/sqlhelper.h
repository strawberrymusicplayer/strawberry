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

#ifndef SQLHELPER_H
#define SQLHELPER_H

#include <QVariant>
#include <QString>
#include <QUrl>

class SqlHelper {
 public:
  template <typename T>
  static QString ValueToString(const T &q, const int n);

  template <typename T>
  static QUrl ValueToUrl(const T &q, const int n);

  template <typename T>
  static int ValueToInt(const T &q, const int n);

  template <typename T>
  static uint ValueToUInt(const T &q, const int n);

  template <typename T>
  static qint64 ValueToLongLong(const T &q, const int n);

  template <typename T>
  static float ValueToFloat(const T &q, const int n);

  template <typename T>
  static bool ValueToBool(const T &q, const int n);
};

template <typename T>
QString SqlHelper::ValueToString(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() ? QString() : q.value(n).toString();

}

template <typename T>
QUrl SqlHelper::ValueToUrl(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() ? QUrl() : QUrl(q.value(n).toString());

}

template <typename T>
int SqlHelper::ValueToInt(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() ? -1 : q.value(n).toInt();

}

template <typename T>
uint SqlHelper::ValueToUInt(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() || q.value(n).toInt() < 0 ? 0 : q.value(n).toUInt();

}

template <typename T>
qint64 SqlHelper::ValueToLongLong(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() ? -1 : q.value(n).toLongLong();

}

template <typename T>
float SqlHelper::ValueToFloat(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return q.value(n).isNull() ? -1.0F : q.value(n).toFloat();

}

template <typename T>
bool SqlHelper::ValueToBool(const T &q, const int n) {

  Q_ASSERT(n < q.count());

  return !q.value(n).isNull() && q.value(n).toInt() == 1;

}

#endif  // SQLHELPER_H
