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

  enum Type {
    Type_LoadingIndicator,
    Type_Root,
    Type_Service,
    Type_Channel
  };

  explicit RadioItem(SimpleTreeModel<RadioItem> *_model) : SimpleTreeItem<RadioItem>(Type_Root, _model) {}
  explicit RadioItem(Type _type, RadioItem *_parent = nullptr) : SimpleTreeItem<RadioItem>(_type, _parent) {}

  Song::Source source;
  RadioChannel channel;

 private:
  Q_DISABLE_COPY(RadioItem)
};

#endif  // RADIOITEM_H
