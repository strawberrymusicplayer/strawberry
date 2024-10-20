/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <utility>

#include <QtGlobal>
#include <QMetaType>
#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>

#include "core/sqlquery.h"
#include "core/song.h"

#include "collectionquery.h"
#include "collectionfilteroptions.h"

using namespace Qt::Literals::StringLiterals;

CollectionQuery::CollectionQuery(const QSqlDatabase &db, const QString &songs_table, const CollectionFilterOptions &filter_options)
    : SqlQuery(db),
      songs_table_(songs_table),
      include_unavailable_(false),
      duplicates_only_(false),
      limit_(-1) {

  if (filter_options.max_age() != -1) {
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - filter_options.max_age();

    where_clauses_ << u"ctime > ?"_s;
    bound_values_ << cutoff;
  }

  duplicates_only_ = filter_options.filter_mode() == CollectionFilterOptions::FilterMode::Duplicates;

  if (filter_options.filter_mode() == CollectionFilterOptions::FilterMode::Untagged) {
    where_clauses_ << u"(artist = '' OR album = '' OR title ='')"_s;
  }

}

void CollectionQuery::AddWhere(const QString &column, const QVariant &value, const QString &op) {

  // Ignore 'literal' for IN
  if (op.compare("IN"_L1, Qt::CaseInsensitive) == 0) {
    const QStringList values = value.toStringList();
    QStringList final_values;
    final_values.reserve(values.count());
    for (const QString &single_value : values) {
      final_values.append(u"?"_s);
      bound_values_ << single_value;
    }

    where_clauses_ << QStringLiteral("%1 IN (%2)").arg(column, final_values.join(u','));
  }
  else {
    // Do integers inline - sqlite seems to get confused when you pass integers to bound parameters
    if (value.metaType().id() == QMetaType::Int) {
      where_clauses_ << QStringLiteral("%1 %2 %3").arg(column, op, value.toString());
    }
    else if (value.metaType().id() == QMetaType::QString && value.toString().isNull()) {
      where_clauses_ << QStringLiteral("%1 %2 ?").arg(column, op);
      bound_values_ << ""_L1;
    }
    else {
      where_clauses_ << QStringLiteral("%1 %2 ?").arg(column, op);
      bound_values_ << value;
    }
  }

}

void CollectionQuery::AddCompilationRequirement(const bool compilation) {
  // The unary + is added to prevent sqlite from using the index idx_comp_artist.
  where_clauses_ << QStringLiteral("+compilation_effective = %1").arg(compilation ? 1 : 0);

}

QString CollectionQuery::GetInnerQuery() const {
  return duplicates_only_
             ? QStringLiteral(" INNER JOIN (select * from duplicated_songs) dsongs        "
                   "ON (%songs_table.artist = dsongs.dup_artist       "
                   "AND %songs_table.album = dsongs.dup_album     "
                   "AND %songs_table.title = dsongs.dup_title)    ")
             : QString();
}

bool CollectionQuery::Exec() {

  QString sql = QStringLiteral("SELECT %1 FROM %2 %3").arg(column_spec_, songs_table_, GetInnerQuery());

  QStringList where_clauses(where_clauses_);
  if (!include_unavailable_) {
    where_clauses << u"unavailable = 0"_s;
  }

  if (!where_clauses.isEmpty()) sql += " WHERE "_L1 + where_clauses.join(" AND "_L1);

  if (!order_by_.isEmpty()) sql += " ORDER BY "_L1 + order_by_;

  if (limit_ != -1) sql += " LIMIT "_L1 + QString::number(limit_);

  sql.replace("%songs_table"_L1, songs_table_);

  if (!QSqlQuery::prepare(sql)) return false;

  // Bind values
  for (const QVariant &value : std::as_const(bound_values_)) {
    QSqlQuery::addBindValue(value);
  }

  return QSqlQuery::exec();

}

bool CollectionQuery::Next() { return QSqlQuery::next(); }

QVariant CollectionQuery::Value(const int column) const { return QSqlQuery::value(column); }
