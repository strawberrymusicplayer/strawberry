/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QMap>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "sqlquery.h"

using namespace Qt::Literals::StringLiterals;

void SqlQuery::BindValue(const QString &placeholder, const QVariant &value) {

  bound_values_.insert(placeholder, value);

  bindValue(placeholder, value);

}

void SqlQuery::BindStringValue(const QString &placeholder, const QString &value) {

  BindValue(placeholder, value.isNull() ? ""_L1 : value);

}

void SqlQuery::BindUrlValue(const QString &placeholder, const QUrl &value) {

  BindValue(placeholder, value.isValid() ? value.toString(QUrl::FullyEncoded) : ""_L1);

}

void SqlQuery::BindIntValue(const QString &placeholder, const int value) {

  BindValue(placeholder, value <= 0 ? -1 : value);

}

void SqlQuery::BindLongLongValue(const QString &placeholder, const qint64 value) {

  BindValue(placeholder, value <= 0 ? -1 : value);

}

void SqlQuery::BindLongLongValueOrZero(const QString &placeholder, const qint64 value) {

  BindValue(placeholder, value <= 0 ? 0 : value);

}

void SqlQuery::BindFloatValue(const QString &placeholder, const float value) {

  BindValue(placeholder, value <= 0 ? -1 : value);

}

void SqlQuery::BindDoubleOrNullValue(const QString &placeholder, const std::optional<double> value) {

  BindValue(placeholder, value.has_value() ? *value : QVariant());

}

void SqlQuery::BindBoolValue(const QString &placeholder, const bool value) {

  BindValue(placeholder, value ? 1 : 0);

}

void SqlQuery::BindNotNullIntValue(const QString &placeholder, const int value) {

  BindValue(placeholder, value == -1 ? QVariant() : value);

}

bool SqlQuery::Exec() {

  bool success = exec();
  last_query_ = executedQuery();

  for (QMap<QString, QVariant>::const_iterator it = bound_values_.constBegin(); it != bound_values_.constEnd(); ++it) {
    last_query_.replace(it.key(), it.value().toString());
  }
  bound_values_.clear();

  return success;

}

QString SqlQuery::LastQuery() const {

  return last_query_;

}
