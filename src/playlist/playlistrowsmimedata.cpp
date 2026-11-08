/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QModelIndexList>
#include <QPersistentModelIndex>

#include "playlist.h"
#include "playlistrowsmimedata.h"

PlaylistRowsMimeData::PlaylistRowsMimeData(Playlist *playlist, const QModelIndexList &indexes, QObject *parent)
    : QMimeData(),
      playlist_(playlist) {

  setParent(parent);

  indexes_.reserve(indexes.count());
  for (const QModelIndex &idx : indexes) {
    indexes_ << QPersistentModelIndex(idx);
  }

}

Playlist *PlaylistRowsMimeData::playlist() const {

  return playlist_;

}

QList<int> PlaylistRowsMimeData::rows() const {

  QList<int> rows;
  rows.reserve(indexes_.count());
  for (const QPersistentModelIndex &idx : indexes_) {
    if (idx.isValid()) {
      rows << idx.row();
    }
  }

  return rows;

}
