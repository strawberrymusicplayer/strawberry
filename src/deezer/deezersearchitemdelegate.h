/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef DEEZERSEARCHITEMDELEGATE_H
#define DEEZERSEARCHITEMDELEGATE_H

#include <QPainter>
#include <QStyleOptionViewItem>

#include "collection/collectionview.h"

class DeezerSearchView;

class DeezerSearchItemDelegate : public CollectionItemDelegate {
 public:
  DeezerSearchItemDelegate(DeezerSearchView *view);

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

 private:
  DeezerSearchView* view_;
};

#endif  // DEEZERSEARCHITEMDELEGATE_H
