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

#ifndef PLAYLISTUNDOCOMMANDREMOVEITEMS_H
#define PLAYLISTUNDOCOMMANDREMOVEITEMS_H

#include <QList>

#include "playlistundocommandbase.h"
#include "playlistitem.h"

class PlaylistUndoCommandRemoveItems : public PlaylistUndoCommandBase {
 public:
  explicit PlaylistUndoCommandRemoveItems(Playlist *playlist, const int pos, const int count);

  int id() const override { return static_cast<int>(PlaylistUndoCommandBase::Type::RemoveItems); }

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;

 private:
  struct Range {
    Range(const int pos, const int count) : pos_(pos), count_(count) {}
    int pos_;
    int count_;
    PlaylistItemPtrList items_;
  };

  QList<Range> ranges_;
};

#endif // PLAYLISTUNDOCOMMANDREMOVEITEMS_H
