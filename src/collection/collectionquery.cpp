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

#include "config.h"

#include <QtGlobal>
#include <QMetaType>
#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "core/logging.h"
#include "core/song.h"

#include "collectionquery.h"

QueryOptions::QueryOptions() : max_age_(-1), query_mode_(QueryMode_All) {}

CollectionQuery::CollectionQuery(const QSqlDatabase &db, const QString &songs_table, const QueryOptions &options)
    : QSqlQuery(db),
      songs_table_(songs_table),
      include_unavailable_(false),
      duplicates_only_(false),
      limit_(-1) {

  if (options.max_age() != -1) {
    qint64 cutoff = QDateTime::currentDateTime().toSecsSinceEpoch() - options.max_age();

    where_clauses_ << "ctime > ?";
    bound_values_ << cutoff;
  }

  duplicates_only_ = options.query_mode() == QueryOptions::QueryMode_Duplicates;

  if (options.query_mode() == QueryOptions::QueryMode_Untagged) {
    where_clauses_ << "(artist = '' OR album = '' OR title ='')";
  }

}

QString CollectionQuery::GetInnerQuery() const {
  return duplicates_only_
             ? QString(" INNER JOIN (select * from duplicated_songs) dsongs        "
                   "ON (%songs_table.artist = dsongs.dup_artist       "
                   "AND %songs_table.album = dsongs.dup_album     "
                   "AND %songs_table.title = dsongs.dup_title)    ")
             : QString();
}

void CollectionQuery::AddWhere(const QString &column, const QVariant &value, const QString &op) {

  // Ignore 'literal' for IN
  if (op.compare("IN", Qt::CaseInsensitive) == 0) {
    QStringList values = value.toStringList();
    QStringList final;
    final.reserve(values.count());
    for (const QString &single_value : values) {
      final.append("?");
      bound_values_ << single_value;
    }

    where_clauses_ << QString("%1 IN (" + final.join(",") + ")").arg(column);
  }
  else {
    // Do integers inline - sqlite seems to get confused when you pass integers to bound parameters
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (value.metaType().id() == QMetaType::Int) {
#else
    if (value.type() == QVariant::Int) {
#endif
      where_clauses_ << QString("%1 %2 %3").arg(column, op, value.toString());
    }
    else if (
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    value.metaType().id() == QMetaType::QString
#else
    value.type() == QVariant::String
#endif
    && value.toString().isNull()) {
      where_clauses_ << QString("%1 %2 ?").arg(column, op);
      bound_values_ << QString("");
    }
    else {
      where_clauses_ << QString("%1 %2 ?").arg(column, op);
      bound_values_ << value;
    }
  }

}

void CollectionQuery::AddWhereArtist(const QVariant &value) {

  where_clauses_ << QString("((artist = ? AND albumartist = '') OR albumartist = ?)");
  bound_values_ << value;
  bound_values_ << value;

}

void CollectionQuery::AddCompilationRequirement(const bool compilation) {
  // The unary + is added to prevent sqlite from using the index idx_comp_artist.
  where_clauses_ << QString("+compilation_effective = %1").arg(compilation ? 1 : 0);

}

bool CollectionQuery::Exec() {

  QString sql = QString("SELECT %1 FROM %2 %3").arg(column_spec_, songs_table_, GetInnerQuery());

  QStringList where_clauses(where_clauses_);
  if (!include_unavailable_) {
    where_clauses << "unavailable = 0";
  }

  if (!where_clauses.isEmpty()) sql += " WHERE " + where_clauses.join(" AND ");

  if (!order_by_.isEmpty()) sql += " ORDER BY " + order_by_;

  if (limit_ != -1) sql += " LIMIT " + QString::number(limit_);

  sql.replace("%songs_table", songs_table_);

  prepare(sql);

  // Bind values
  for (const QVariant &value : bound_values_) {
    addBindValue(value);
  }

  const bool result = exec();

  if (!result) {
    QSqlError last_error = lastError();
    if (last_error.isValid()) {
      qLog(Error) << "DB error: " << last_error;
      qLog(Error) << "Faulty query: " << lastQuery();
      qLog(Error) << "Bound values: " << boundValues();
    }
  }

  return result;

}

bool CollectionQuery::Next() { return next(); }

QVariant CollectionQuery::Value(const int column) const { return value(column); }

bool QueryOptions::Matches(const Song &song) const {

  if (max_age_ != -1) {
    const qint64 cutoff = QDateTime::currentDateTime().toSecsSinceEpoch() - max_age_;
    if (song.ctime() <= cutoff) return false;
  }

  if (!filter_.isNull()) {
    return song.artist().contains(filter_, Qt::CaseInsensitive) || song.album().contains(filter_, Qt::CaseInsensitive) || song.title().contains(filter_, Qt::CaseInsensitive);
  }

  return true;

}
