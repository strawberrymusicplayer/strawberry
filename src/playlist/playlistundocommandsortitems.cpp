/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>

#include "playlist.h"
#include "playlistundocommandsortitems.h"

PlaylistUndoCommandSortItems::PlaylistUndoCommandSortItems(Playlist *playlist,
                                                           const bool old_is_sorted,
                                                           const Playlist::Column old_column,
                                                           const Qt::SortOrder old_sort_order,
                                                           const bool new_is_sorted,
                                                           const Playlist::Column new_column,
                                                           const Qt::SortOrder new_sort_order,
                                                           const PlaylistItemPtrList &new_items)
    : PlaylistUndoCommandReOrderItems(playlist, new_items),
      old_is_sorted_(old_is_sorted),
      old_column_(old_column),
      old_sort_order_(old_sort_order),
      new_is_sorted_(new_is_sorted),
      new_column_(new_column),
      new_sort_order_(new_sort_order) {

  setText(QObject::tr("sort songs"));

}

void PlaylistUndoCommandSortItems::undo() {

  PlaylistUndoCommandReOrderItems::undo();

  playlist_->is_sorted_ = old_is_sorted_;
  playlist_->sort_column_ = old_column_;
  playlist_->sort_order_ = old_sort_order_;

  Q_EMIT playlist_->SortStateChanged(old_is_sorted_, old_column_, old_sort_order_);

}

void PlaylistUndoCommandSortItems::redo() {

  PlaylistUndoCommandReOrderItems::redo();

  playlist_->is_sorted_ = new_is_sorted_;
  playlist_->sort_column_ = new_column_;
  playlist_->sort_order_ = new_sort_order_;

  Q_EMIT playlist_->SortStateChanged(new_is_sorted_, new_column_, new_sort_order_);

}
