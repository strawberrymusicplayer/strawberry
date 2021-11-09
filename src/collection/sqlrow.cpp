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

#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSqlQuery>
#include <QSqlRecord>

#include "sqlrow.h"

SqlRow::SqlRow(const QSqlQuery &query) { Init(query); }

void SqlRow::Init(const QSqlQuery &query) {

  const QSqlRecord r = query.record();
  for (int i = 0; i < r.count(); ++i) {
    columns_by_number_.insert(i, query.value(i));
    const QString field_name = r.fieldName(i);
    if (!columns_by_name_.contains(field_name) || columns_by_name_[field_name].isNull()) {
      columns_by_name_.insert(field_name, query.value(i));
    }
  }

}

const QVariant SqlRow::value(const int number) const {

  if (columns_by_number_.contains(number)) {
    return columns_by_number_[number];
  }
  else {
    return QVariant();
  }

}

const QVariant SqlRow::value(const QString &name) const {

  if (columns_by_name_.contains(name)) {
    return columns_by_name_[name];
  }
  else {
    return QVariant();
  }

}

QString SqlRow::ValueToString(const QString &n) const {
  return value(n).isNull() ? QString() : value(n).toString();
}

QUrl SqlRow::ValueToUrl(const QString &n) const {
  return value(n).isNull() ? QUrl() : QUrl(value(n).toString());
}

int SqlRow::ValueToInt(const QString &n) const {
  return value(n).isNull() ? -1 : value(n).toInt();
}

uint SqlRow::ValueToUInt(const QString &n) const {
  return value(n).isNull() || value(n).toInt() < 0 ? 0 : value(n).toInt();
}

qint64 SqlRow::ValueToLongLong(const QString &n) const {
  return value(n).isNull() ? -1 : value(n).toLongLong();
}

float SqlRow::ValueToFloat(const QString &n) const {
  return value(n).isNull() ? -1.0F : value(n).toFloat();
}

bool SqlRow::ValueToBool(const QString &n) const {
  return !value(n).isNull() && value(n).toInt() == 1;
}
