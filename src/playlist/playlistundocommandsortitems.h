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

#ifndef PLAYLISTUNDOCOMMANDSORTITEMS_H
#define PLAYLISTUNDOCOMMANDSORTITEMS_H

#include "playlistundocommandreorderitems.h"
#include "playlist.h"
#include "playlistitem.h"

class PlaylistUndoCommandSortItems : public PlaylistUndoCommandReOrderItems {
 public:
  explicit PlaylistUndoCommandSortItems(Playlist *playlist,
                                        const bool old_is_sorted,
                                        const Playlist::Column old_column,
                                        const Qt::SortOrder old_sort_order,
                                        const bool new_is_sorted,
                                        const Playlist::Column new_column,
                                        const Qt::SortOrder new_sort_order,
                                        const PlaylistItemPtrList &new_items);

  void undo() override;
  void redo() override;

 private:
  bool old_is_sorted_;
  Playlist::Column old_column_;
  Qt::SortOrder old_sort_order_;
  bool new_is_sorted_;
  Playlist::Column new_column_;
  Qt::SortOrder new_sort_order_;
};

#endif  // PLAYLISTUNDOCOMMANDSORTITEMS_H
