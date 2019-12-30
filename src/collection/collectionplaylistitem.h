/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"


#include <QVariant>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "playlist/playlistitem.h"

class SqlRow;

class CollectionPlaylistItem : public PlaylistItem {
 public:
  CollectionPlaylistItem();
  CollectionPlaylistItem(const Song &song);

  bool InitFromQuery(const SqlRow &query);
  void Reload();

  Song Metadata() const;
  void SetMetadata(const Song &song) { song_ = song; }

  QUrl Url() const;

  bool IsLocalCollectionItem() const { return true; }

 protected:
  QVariant DatabaseValue(DatabaseColumn column) const;
  Song DatabaseSongMetadata() const { return Song(Song::Source_Collection); }

 protected:
  Song song_;
};

#endif // COLLECTIONPLAYLISTITEM_H

