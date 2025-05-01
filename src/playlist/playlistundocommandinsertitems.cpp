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

#include "playlistundocommandinsertitems.h"
#include "playlist.h"

PlaylistUndoCommandInsertItems::PlaylistUndoCommandInsertItems(Playlist *playlist, const PlaylistItemPtrList &items, const int pos, const bool enqueue, const bool enqueue_next)
    : PlaylistUndoCommandBase(playlist),
      items_(items),
      pos_(pos),
      enqueue_(enqueue),
      enqueue_next_(enqueue_next) {

  setText(QObject::tr("add %n songs", "", static_cast<int>(items_.count())));

}

void PlaylistUndoCommandInsertItems::redo() {
  playlist_->InsertItemsWithoutUndo(items_, pos_, enqueue_, enqueue_next_);
}

void PlaylistUndoCommandInsertItems::undo() {

  const int start = pos_ == -1 ? static_cast<int>(playlist_->rowCount() - items_.count()) : pos_;
  playlist_->RemoveItemsWithoutUndo(start, static_cast<int>(items_.count()));

}

bool PlaylistUndoCommandInsertItems::UpdateItem(const PlaylistItemPtr &updated_item) {

  for (int i = 0; i < items_.size(); i++) {
    PlaylistItemPtr item = items_.value(i);
    if (item->EffectiveMetadata().url() == updated_item->EffectiveMetadata().url()) {
      items_[i] = updated_item;
      return true;
    }
  }
  return false;

}
