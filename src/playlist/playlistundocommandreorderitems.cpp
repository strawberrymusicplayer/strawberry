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

#include "playlist.h"
#include "playlistundocommandreorderitems.h"

PlaylistUndoCommandReOrderItems::PlaylistUndoCommandReOrderItems(Playlist *playlist, const PlaylistItemPtrList &new_items)
    : PlaylistUndoCommandBase(playlist), old_items_(playlist->items_), new_items_(new_items) {}

void PlaylistUndoCommandReOrderItems::undo() { playlist_->ReOrderWithoutUndo(old_items_); }

void PlaylistUndoCommandReOrderItems::redo() { playlist_->ReOrderWithoutUndo(new_items_); }
