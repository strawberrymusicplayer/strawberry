/*
 * Strawberry Music Player
 * Copyright 2020-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTUNDOCOMMANDREORDERITEMS_H
#define PLAYLISTUNDOCOMMANDREORDERITEMS_H

#include "playlistundocommandbase.h"
#include "playlistitem.h"

class Playlist;

class PlaylistUndoCommandReOrderItems : public PlaylistUndoCommandBase {
 public:
  explicit PlaylistUndoCommandReOrderItems(Playlist *playlist, const PlaylistItemPtrList &new_items);

  void undo() override;
  void redo() override;

  // Replaces old_item with new_item in the stored item orders, so undo and redo keep an item that was replaced after the command was pushed.
  void ReplaceItem(const PlaylistItemPtr &old_item, const PlaylistItemPtr &new_item);

 private:
  PlaylistItemPtrList old_items_;
  PlaylistItemPtrList new_items_;
};

#endif  // PLAYLISTUNDOCOMMANDREORDERITEMS_H
