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

#include "playlistlistsortfiltermodel.h"

PlaylistListSortFilterModel::PlaylistListSortFilterModel(QObject *parent)
      : QSortFilterProxyModel(parent) {}

bool PlaylistListSortFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {

    // Compare the display text first.
    const int ret = left.data().toString().localeAwareCompare(right.data().toString());
    if (ret < 0) return true;
    if (ret > 0) return false;

    // Now use the source model row order to ensure we always get a deterministic sorting even when two items are named the same.
    return left.row() < right.row();
}
