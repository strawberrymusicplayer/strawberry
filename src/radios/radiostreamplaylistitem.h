/*
 * Strawberry Music Player
 * Copyright 2021-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOSTREAMPLAYLISTITEM_H
#define RADIOSTREAMPLAYLISTITEM_H

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "playlist/streamplaylistitem.h"

class RadioService;

class RadioStreamPlaylistItem : public StreamPlaylistItem {
 public:
  explicit RadioStreamPlaylistItem(const Song &song);
  explicit RadioStreamPlaylistItem(const SharedPtr<RadioService> service, const Song &song);
  Q_DISABLE_COPY(RadioStreamPlaylistItem)

 private:
  const SharedPtr<RadioService> service_;
};

#endif  // RADIOSTREAMPLAYLISTITEM_H
