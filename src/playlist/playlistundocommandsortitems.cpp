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
#include "playlistundocommandsortitems.h"

PlaylistUndoCommandSortItems::PlaylistUndoCommandSortItems(Playlist *playlist, const Playlist::Column column, const Qt::SortOrder order, const PlaylistItemPtrList &new_items)
    : PlaylistUndoCommandReOrderItems(playlist, new_items) {

  Q_UNUSED(column);
  Q_UNUSED(order);

  setText(QObject::tr("sort songs"));

}
