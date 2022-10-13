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

#ifndef SQLROW_H
#define SQLROW_H

#include "config.h"

#include <QList>
#include <QVariant>
#include <QUrl>
#include <QSqlQuery>

class SqlRow {

 public:
  SqlRow(const QSqlQuery &query);

  const QVariant value(const int number) const;
  const QVariant value(const QString &name) const;

  QString ValueToString(const QString &n) const;
  QUrl ValueToUrl(const QString &n) const;
  int ValueToInt(const QString &n) const;
  uint ValueToUInt(const QString &n) const;
  qint64 ValueToLongLong(const QString &n) const;
  float ValueToFloat(const QString &n) const;
  bool ValueToBool(const QString& n) const;

 private:
  SqlRow();

  void Init(const QSqlQuery &query);

  QMap<int, QVariant> columns_by_number_;
  QMap<QString, QVariant> columns_by_name_;

};

using SqlRowList = QList<SqlRow>;

#endif  // SQLROW_H
