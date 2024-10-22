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

#ifndef FILTERTREEOR_H
#define FILTERTREEOR_H

#include <QList>

#include "filtertree.h"

#include "core/song.h"

class FilterTreeOr : public FilterTree {
 public:
  explicit FilterTreeOr();
  ~FilterTreeOr() override;

  FilterType type() const override { return FilterType::Or; }
  virtual void add(FilterTree *child);
  bool accept(const Song &song) const override;

 private:
  QList<FilterTree*> children_;

  Q_DISABLE_COPY(FilterTreeOr)
};

#endif  // FILTERTREEOR_H
