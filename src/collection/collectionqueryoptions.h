/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONQUERYOPTIONS_H
#define COLLECTIONQUERYOPTIONS_H

#include <QList>
#include <QVariant>
#include <QString>

class CollectionQueryOptions {
 public:

  explicit CollectionQueryOptions();

  struct Where {
    explicit Where(const QString &_column = QString(), const QVariant &_value = QString(), const QString &_op = QString()) : column(_column), value(_value), op(_op) {}
    QString column;
    QVariant value;
    QString op;
  };

  enum class CompilationRequirement {
    None,
    On,
    Off
  };

  QString column_spec() const { return column_spec_; }
  CompilationRequirement compilation_requirement() const { return compilation_requirement_; }
  bool query_have_compilations() const { return query_have_compilations_; }

  void set_column_spec(const QString &column_spec) { column_spec_ = column_spec; }
  void set_compilation_requirement(const CompilationRequirement compilation_requirement) { compilation_requirement_ = compilation_requirement; }
  void set_query_have_compilations(const bool query_have_compilations) { query_have_compilations_ = query_have_compilations; }

  QList<Where> where_clauses() const { return where_clauses_; }
  void AddWhere(const QString &column, const QVariant &value, const QString &op = "=");

 private:
  QString column_spec_;
  CompilationRequirement compilation_requirement_;
  bool query_have_compilations_;
  QList<Where> where_clauses_;
};

#endif  // COLLECTIONQUERYOPTIONS_H
