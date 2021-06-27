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

#ifndef SMARTPLAYLISTSITEM_H
#define SMARTPLAYLISTSITEM_H

#include "config.h"

#include <QMetaType>
#include <QByteArray>

#include "core/simpletreeitem.h"
#include "playlistgenerator.h"

class SmartPlaylistsItem : public SimpleTreeItem<SmartPlaylistsItem> {
 public:
  enum class Type {
    Root,
    SmartPlaylist
  };

  SmartPlaylistsItem(SimpleTreeModel<SmartPlaylistsItem> *_model) : SimpleTreeItem<SmartPlaylistsItem>(_model), type(Type::Root) {}
  SmartPlaylistsItem(const Type _type, SmartPlaylistsItem *_parent = nullptr) : SimpleTreeItem<SmartPlaylistsItem>(_parent), type(_type) {}

  Type type;
  PlaylistGenerator::Type smart_playlist_type;
  QByteArray smart_playlist_data;

  Q_DISABLE_COPY(SmartPlaylistsItem)
};

Q_DECLARE_METATYPE(SmartPlaylistsItem::Type)

#endif  // SMARTPLAYLISTSITEM_H
