/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "collectionitem.h"

CollectionItem::CollectionItem(SimpleTreeModel<CollectionItem> *_model)
    : SimpleTreeItem<CollectionItem>(_model),
      type(Type::Root),
      container_level(-1),
      compilation_artist_node_(nullptr) {}

CollectionItem::CollectionItem(const Type _type, CollectionItem *_parent)
    : SimpleTreeItem<CollectionItem>(_parent),
      type(_type),
      container_level(-1),
      compilation_artist_node_(nullptr) {}
