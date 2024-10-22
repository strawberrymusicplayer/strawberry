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

#ifndef PLAYLISTUNDOCOMMANDINSERTITEMS_H
#define PLAYLISTUNDOCOMMANDINSERTITEMS_H

#include "playlistundocommandbase.h"
#include "playlistitem.h"

class PlaylistUndoCommandInsertItems : public PlaylistUndoCommandBase {
 public:
  explicit PlaylistUndoCommandInsertItems(Playlist *playlist, const PlaylistItemPtrList &items, const int pos, const bool enqueue = false, const bool enqueue_next = false);

  void undo() override;
  void redo() override;
  // When load is async, items have already been pushed, so we need to update them.
  // This function try to find the equivalent item, and replace it with the new (completely loaded) one.
  // Return true if the was found (and updated), false otherwise
  bool UpdateItem(const PlaylistItemPtr &updated_item);

 private:
  PlaylistItemPtrList items_;
  int pos_;
  bool enqueue_;
  bool enqueue_next_;
};

#endif // PLAYLISTUNDOCOMMANDINSERTITEMS_H
