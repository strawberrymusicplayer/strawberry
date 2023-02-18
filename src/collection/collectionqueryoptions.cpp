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

#include <QVariant>
#include <QString>

#include "collectionqueryoptions.h"
#include "collectionfilteroptions.h"

CollectionQueryOptions::CollectionQueryOptions()
    : compilation_requirement_(CollectionQueryOptions::CompilationRequirement::None),
      query_have_compilations_(false) {}

void CollectionQueryOptions::AddWhere(const QString &column, const QVariant &value, const QString &op) {

  where_clauses_ << Where(column, value, op);

}
