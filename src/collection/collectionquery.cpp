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

#include "core/song.h"

#include "collectionquery.h"
#include "collectionfilteroptions.h"
#include "utilities/searchparserutils.h"

CollectionQuery::CollectionQuery(const QSqlDatabase &db, const QString &songs_table, const QString &fts_table, const CollectionFilterOptions &filter_options)
    : QSqlQuery(db),
      songs_table_(songs_table),
      fts_table_(fts_table),
      include_unavailable_(false),
      join_with_fts_(false),
      duplicates_only_(false),
      limit_(-1) {

  if (!filter_options.filter_text().isEmpty()) {
    // We need to munge the filter text a little bit to get it to work as expected with sqlite's FTS5:
    //  1) Append * to all tokens.
    //  2) Prefix "fts" to column names.
    //  3) Remove colons which don't correspond to column names.

    // Split on whitespace
    QString filter_text = filter_options.filter_text().replace(QRegularExpression(":\\s+"), ":");
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList tokens(filter_text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts));
#else
    QStringList tokens(filter_text.split(QRegularExpression("\\s+"), QString::SkipEmptyParts));
#endif
    QString query;
    for (QString token : tokens) {
      token.remove('(')
           .remove(')')
           .remove('"')
           .replace('-', ' ');

      if (token.contains(':')) {
        const QString columntoken = token.section(':', 0, 0);
        QString subtoken = token.section(':', 1, -1).replace(":", " ").trimmed();
        if (subtoken.isEmpty()) continue;
        if (Song::kFtsColumns.contains("fts" + columntoken, Qt::CaseInsensitive)) {
          if (!query.isEmpty()) query.append(" ");
          query += "fts" + columntoken + ":\"" + subtoken + "\"*";
        }
        else if (Song::kNumericalColumns.contains(columntoken, Qt::CaseInsensitive)) {
          QString comparator = RemoveSqlOperator(subtoken);
          if (columntoken.compare("rating", Qt::CaseInsensitive) == 0) {
            AddWhereRating(subtoken, comparator);
          }
          else if (columntoken.compare("length", Qt::CaseInsensitive) == 0) {
            // Time is saved in nanoseconds, so add 9 0's
            QString parsedTime = QString::number(Utilities::ParseSearchTime(subtoken)) + "000000000";
            AddWhere(columntoken, parsedTime, comparator);
          }
          else {
            AddWhere(columntoken, subtoken, comparator);
          }
        }
        // Not a valid filter, remove
        else {
          token = token.replace(":", " ").trimmed();
          if (!token.isEmpty()) {
            if (!query.isEmpty()) query.append(" ");
            query += "\"" + token + "\"*";
          }
        }
      }
      else {
        if (!query.isEmpty()) query.append(" ");
        query += "\"" + token + "\"*";
      }
    }
    if (!query.isEmpty()) {
      where_clauses_ << "fts.%fts_table_noprefix MATCH ?";
      bound_values_ << query;
      join_with_fts_ = true;
    }
  }

  if (filter_options.max_age() != -1) {
    qint64 cutoff = QDateTime::currentDateTime().toSecsSinceEpoch() - filter_options.max_age();

    where_clauses_ << "ctime > ?";
    bound_values_ << cutoff;
  }

  // TODO: Currently you cannot use any FilterMode other than All and FTS at the same time.
  // Joining songs, duplicated_songs and songs_fts all together takes a huge amount of time.
  // The query takes about 20 seconds on my machine then. Why?
  // Untagged mode could work with additional filtering but I'm disabling it just to be consistent
  // this way filtering is available only in the All mode.
  // Remember though that when you fix the Duplicates + FTS cooperation, enable the filtering in both Duplicates and Untagged modes.
  duplicates_only_ = filter_options.filter_mode() == CollectionFilterOptions::FilterMode::Duplicates;

  if (filter_options.filter_mode() == CollectionFilterOptions::FilterMode::Untagged) {
    where_clauses_ << "(artist = '' OR album = '' OR title ='')";
  }

}

QString CollectionQuery::RemoveSqlOperator(QString &token) {

  QString op = "=";
  static QRegularExpression rxOp("^(=|<[>=]?|>=?|!=)");
  QRegularExpressionMatch match = rxOp.match(token);
  if (match.hasMatch()) {
    op = match.captured(0);
  }
  token.remove(rxOp);

  if (op == "!=") {
    op = "<>";
  }

  return op;

}

void CollectionQuery::AddWhere(const QString &column, const QVariant &value, const QString &op) {

  // Ignore 'literal' for IN
  if (op.compare("IN", Qt::CaseInsensitive) == 0) {
    QStringList values = value.toStringList();
    QStringList final_values;
    final_values.reserve(values.count());
    for (const QString &single_value : values) {
      final_values.append("?");
      bound_values_ << single_value;
    }

    where_clauses_ << QString("%1 IN (" + final_values.join(",") + ")").arg(column);
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

void CollectionQuery::AddWhereRating(const QVariant &value, const QString &op) {

  float parsed_rating = Utilities::ParseSearchRating(value.toString());

  // You can't query the database for a float, due to float precision errors,
  // So we have to use a certain tolerance, so that the searched value is definetly included.
  const float tolerance = 0.001F;
  if (op == "<") {
    AddWhere("rating", parsed_rating-tolerance, "<");
  }
  else if (op == ">") {
    AddWhere("rating", parsed_rating+tolerance, ">");
  }
  else if (op == "<=") {
    AddWhere("rating", parsed_rating+tolerance, "<=");
  }
  else if (op == ">=") {
    AddWhere("rating", parsed_rating-tolerance, ">=");
  }
  else if (op == "<>") {
    where_clauses_ << QString("(rating<? OR rating>?)");
    bound_values_ << parsed_rating - tolerance;
    bound_values_ << parsed_rating + tolerance;
  }
  else /* (op == "=") */ {
    AddWhere("rating", parsed_rating+tolerance, "<");
    AddWhere("rating", parsed_rating-tolerance, ">");
  }

}

void CollectionQuery::AddCompilationRequirement(const bool compilation) {
  // The unary + is added to prevent sqlite from using the index idx_comp_artist.
  // When joining with fts, sqlite 3.8 has a tendency to use this index and thereby nesting the tables in an order which gives very poor performance

  where_clauses_ << QString("+compilation_effective = %1").arg(compilation ? 1 : 0);

}

QString CollectionQuery::GetInnerQuery() const {
  return duplicates_only_
             ? QString(" INNER JOIN (select * from duplicated_songs) dsongs        "
                   "ON (%songs_table.artist = dsongs.dup_artist       "
                   "AND %songs_table.album = dsongs.dup_album     "
                   "AND %songs_table.title = dsongs.dup_title)    ")
             : QString();
}

bool CollectionQuery::Exec() {

  QString sql;

  if (join_with_fts_) {
    sql = QString("SELECT %1 FROM %2 INNER JOIN %3 AS fts ON %2.ROWID = fts.ROWID").arg(column_spec_, songs_table_, fts_table_);
  }
  else {
    sql = QString("SELECT %1 FROM %2 %3").arg(column_spec_, songs_table_, GetInnerQuery());
  }

  QStringList where_clauses(where_clauses_);
  if (!include_unavailable_) {
    where_clauses << "unavailable = 0";
  }

  if (!where_clauses.isEmpty()) sql += " WHERE " + where_clauses.join(" AND ");

  if (!order_by_.isEmpty()) sql += " ORDER BY " + order_by_;

  if (limit_ != -1) sql += " LIMIT " + QString::number(limit_);

  sql.replace("%songs_table", songs_table_);
  sql.replace("%fts_table_noprefix", fts_table_.section('.', -1, -1));
  sql.replace("%fts_table", fts_table_);

  if (!QSqlQuery::prepare(sql)) return false;

  // Bind values
  for (const QVariant &value : bound_values_) {
    QSqlQuery::addBindValue(value);
  }

  return QSqlQuery::exec();

}

bool CollectionQuery::Next() { return QSqlQuery::next(); }

QVariant CollectionQuery::Value(const int column) const { return QSqlQuery::value(column); }
