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

#include <QObject>

#include "playlist.h"
#include "playlistundocommandshuffleitems.h"

PlaylistUndoCommandShuffleItems::PlaylistUndoCommandShuffleItems(Playlist *playlist, const PlaylistItemPtrList &new_items)
    : PlaylistUndoCommandReOrderItems(playlist, new_items) {

  setText(QObject::tr("shuffle songs"));

}
