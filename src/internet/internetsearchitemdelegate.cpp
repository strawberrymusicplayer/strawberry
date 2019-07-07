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

#include <QStyleOptionViewItem>
#include <QPainter>

#include "internetsearchitemdelegate.h"
#include "internetsearchview.h"

InternetSearchItemDelegate::InternetSearchItemDelegate(InternetSearchView *view)
    : CollectionItemDelegate(view), view_(view) {}

void InternetSearchItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  // Tell the view we painted this item so it can lazy load some art.
  const_cast<InternetSearchView*>(view_)->LazyLoadAlbumCover(index);

  CollectionItemDelegate::paint(painter, option, index);

}
