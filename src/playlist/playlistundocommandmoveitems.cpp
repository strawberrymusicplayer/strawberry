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
#include "playlistundocommandmoveitems.h"

PlaylistUndoCommandMoveItems::PlaylistUndoCommandMoveItems(Playlist *playlist, const QList<int> &source_rows, const int pos)
    : PlaylistUndoCommandBase(playlist),
      source_rows_(source_rows),
      pos_(pos) {

  setText(QObject::tr("move %n songs", "", static_cast<int>(source_rows.count())));

}

void PlaylistUndoCommandMoveItems::redo() {
  playlist_->MoveItemsWithoutUndo(source_rows_, pos_);
}

void PlaylistUndoCommandMoveItems::undo() {
  playlist_->MoveItemsWithoutUndo(pos_, source_rows_);
}
