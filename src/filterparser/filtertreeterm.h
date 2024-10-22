/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef FILTERTREETERM_H
#define FILTERTREETERM_H

#include <QScopedPointer>

#include "filtertree.h"

#include "core/song.h"

class FilterParserSearchTermComparator;

// Filter that applies a SearchTermComparator to all fields
class FilterTreeTerm : public FilterTree {
 public:
  explicit FilterTreeTerm(FilterParserSearchTermComparator *comparator);

  FilterType type() const override { return FilterType::Term; }
  bool accept(const Song &song) const override;

 private:
  QScopedPointer<FilterParserSearchTermComparator> cmp_;

  Q_DISABLE_COPY(FilterTreeTerm)
};

#endif  // FILTERTREETERM_H
