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
#include "playlistundocommandremoveitems.h"

PlaylistUndoCommandRemoveItems::PlaylistUndoCommandRemoveItems(Playlist *playlist, const int pos, const int count) : PlaylistUndoCommandBase(playlist) {
  setText(QObject::tr("remove %n songs", "", count));

  ranges_ << Range(pos, count);
}

void PlaylistUndoCommandRemoveItems::redo() {

  for (int i = 0; i < ranges_.count(); ++i) {
    ranges_[i].items_ = playlist_->RemoveItemsWithoutUndo(ranges_[i].pos_, ranges_[i].count_);
  }

}

void PlaylistUndoCommandRemoveItems::undo() {

  for (int i = static_cast<int>(ranges_.count() - 1); i >= 0; --i) {
    playlist_->InsertItemsWithoutUndo(ranges_[i].items_, ranges_[i].pos_);
  }

}

bool PlaylistUndoCommandRemoveItems::mergeWith(const QUndoCommand *other) {

  const PlaylistUndoCommandRemoveItems *remove_command = static_cast<const PlaylistUndoCommandRemoveItems*>(other);
  ranges_.append(remove_command->ranges_);

  int sum = 0;
  for (const Range &range : std::as_const(ranges_)) sum += range.count_;
  setText(QObject::tr("remove %n songs", "", sum));

  return true;

}
