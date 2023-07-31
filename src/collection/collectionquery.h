/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef COLLECTIONQUERY_H
#define COLLECTIONQUERY_H

#include "config.h"

#include <QMetaType>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "collectionfilteroptions.h"

class CollectionQuery : public QSqlQuery {
 public:
  explicit CollectionQuery(const QSqlDatabase &db, const QString &songs_table, const QString &fts_table, const CollectionFilterOptions &filter_options = CollectionFilterOptions());

  QVariant Value(const int column) const;
  QVariant value(const int column) const { return Value(column); }

  bool Exec();
  bool exec() { return QSqlQuery::exec(); }

  bool Next();

  QString column_spec() const { return column_spec_; }
  QString order_by() const { return order_by_; }
  QStringList where_clauses() const { return where_clauses_; }
  QVariantList bound_values() const { return bound_values_; }
  bool include_unavailable() const { return include_unavailable_; }
  bool join_with_fts() const { return join_with_fts_; }
  bool duplicates_only() const { return duplicates_only_; }
  int limit() const { return limit_; }

  // Sets contents of SELECT clause on the query (list of columns to get).
  void SetColumnSpec(const QString &column_spec) { column_spec_ = column_spec; }

  // Sets an ORDER BY clause on the query.
  void SetOrderBy(const QString &order_by) { order_by_ = order_by; }

  void SetWhereClauses(const QStringList &where_clauses) { where_clauses_ = where_clauses; }

  // Removes = < > <= >= <> from the beginning of the input string and returns the operator
  // If the input String has no operator, returns "="
  QString RemoveSqlOperator(QString &token);
  // Adds a fragment of WHERE clause. When executed, this Query will connect all the fragments with AND operator.
  // Please note that IN operator expects a QStringList as value.
  void AddWhere(const QString &column, const QVariant &value, const QString &op = "=");
  void AddWhereArtist(const QVariant &value);
  void AddWhereRating(const QVariant &value, const QString &op = "=");

  void SetBoundValues(const QVariantList &bound_values) { bound_values_ = bound_values; }
  void SetDuplicatesOnly(const bool duplicates_only) { duplicates_only_ = duplicates_only; }
  void SetIncludeUnavailable(const bool include_unavailable) { include_unavailable_ = include_unavailable; }
  void SetLimit(const int limit) { limit_ = limit; }
  void AddCompilationRequirement(const bool compilation);

 private:
  QString GetInnerQuery() const;

  QSqlDatabase db_;
  QString songs_table_;
  QString fts_table_;

  QString column_spec_;
  QString order_by_;
  QStringList where_clauses_;
  QVariantList bound_values_;

  bool include_unavailable_;
  bool join_with_fts_;
  bool duplicates_only_;
  int limit_;
};

#endif  // COLLECTIONQUERY_H
