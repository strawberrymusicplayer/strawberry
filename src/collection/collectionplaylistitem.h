/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONPLAYLISTITEM_H
#define COLLECTIONPLAYLISTITEM_H

#include "core/song.h"
#include "playlist/databaseplaylistitem.h"

class CollectionPlaylistItem : public DatabasePlaylistItem {
 public:
  explicit CollectionPlaylistItem();
  explicit CollectionPlaylistItem(const Song &song);

  bool IsLocalCollectionItem() const override { return true; }

 private:
  Q_DISABLE_COPY(CollectionPlaylistItem)
};

#endif  // COLLECTIONPLAYLISTITEM_H
