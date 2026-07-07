/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOITEM_H
#define RADIOITEM_H

#include "config.h"

#include "core/simpletreeitem.h"
#include "core/song.h"
#include "radiochannel.h"

class RadioItem : public SimpleTreeItem<RadioItem> {
 public:

  enum class Type {
    LoadingIndicator,
    Root,
    Service,
    Channel
  };

  explicit RadioItem(SimpleTreeModel<RadioItem> *_model) : SimpleTreeItem<RadioItem>(_model), type(Type::Root), source(Song::Source::Unknown) {}
  explicit RadioItem(const Type _type, RadioItem *_parent = nullptr) : SimpleTreeItem<RadioItem>(_parent), type(_type), source(Song::Source::Unknown) {}

  Type type;
  Song::Source source;
  RadioChannel channel;

 private:
  Q_DISABLE_COPY(RadioItem)
};

Q_DECLARE_METATYPE(RadioItem::Type)

#endif  // RADIOITEM_H
