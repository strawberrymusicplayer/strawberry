/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QSqlQuery>

class CollectionQuery;

class SqlRow {

 public:
  // WARNING: Implicit construction from QSqlQuery and CollectionQuery.
  SqlRow(const QSqlQuery &query);
  SqlRow(const CollectionQuery &query);

  const QVariant &value(int i) const { return columns_[i]; }

  QList<QVariant> columns_;

 private:
  SqlRow();

  void Init(const QSqlQuery &query);

};

typedef QList<SqlRow> SqlRowList;

#endif

