/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILTERTREE_H
#define FILTERTREE_H

#include <QString>

#include "core/song.h"
#include "filtercolumn.h"

class FilterTree {
 public:
  explicit FilterTree();
  virtual ~FilterTree();

  enum class FilterType {
    Nop = 0,
    Or,
    And,
    Not,
    Column,
    Term
  };

  virtual FilterType type() const = 0;

  virtual bool accept(const Song &song) const = 0;

 protected:
  static QVariant DataFromColumn(const FilterColumn filter_column, const Song &metadata);

 private:
  Q_DISABLE_COPY(FilterTree)
};

#endif  // FILTERTREE_H
