/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QSqlRecord>

#include "sqlquery.h"

class SqlRow {

 public:
  explicit SqlRow(const SqlQuery &query);

  int columns() const { return record_.count(); }
  const QSqlRecord &record() const { return record_; }
  QVariant value(const int n) const;

 private:
  void Init(const SqlQuery &query);

  QSqlRecord record_;
};

using SqlRowList = QList<SqlRow>;

#endif  // SQLROW_H
