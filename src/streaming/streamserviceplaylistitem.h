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

#ifndef STREAMSERVICEPLAYLISTITEM_H
#define STREAMSERVICEPLAYLISTITEM_H

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "playlist/streamplaylistitem.h"

class StreamingService;

class StreamServicePlaylistItem : public StreamPlaylistItem {
 public:
  explicit StreamServicePlaylistItem(const Song &song);
  explicit StreamServicePlaylistItem(const SharedPtr<StreamingService> service, const Song &song);
  Q_DISABLE_COPY(StreamServicePlaylistItem)
 private:
  SharedPtr<StreamingService> service_;
};

#endif  // STREAMPLAYLISTITEM_H
