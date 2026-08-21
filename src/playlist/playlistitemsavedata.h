/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTITEMSAVEDATA_H
#define PLAYLISTITEMSAVEDATA_H

#include "config.h"

#include <QMetaType>
#include <QList>
#include <QVariant>
#include <QUuid>

#include "core/song.h"

// Everything PlaylistBackend needs to write one row of a playlist.
// PlaylistBackend runs in the database thread while the playlist model keeps mutating its items on its own thread (inline tag edits, collection updates, stream metadata),
// so PlaylistItem::CreateSaveData() captures the values up front instead of letting the database thread read the items later.
// Song is copy on write with an atomic reference count, so a copy taken on the playlist's thread can safely be read from the database thread.
class PlaylistItemSaveData {
 public:
  explicit PlaylistItemSaveData();

  Song::Source source;
  QUuid uuid;
  QVariant collection_id;
  Song song;
};
using PlaylistItemSaveDataList = QList<PlaylistItemSaveData>;

Q_DECLARE_METATYPE(PlaylistItemSaveData)
Q_DECLARE_METATYPE(PlaylistItemSaveDataList)

#endif  // PLAYLISTITEMSAVEDATA_H
