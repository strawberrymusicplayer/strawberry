/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONAUTOQUEUE_H
#define COLLECTIONAUTOQUEUE_H

#include "config.h"

#include <QObject>

#include "includes/shared_ptr.h"

class PlaylistManager;
class CollectionModel;

class CollectionAutoQueue : public QObject {
  Q_OBJECT

 public:
  explicit CollectionAutoQueue(const SharedPtr<PlaylistManager> playlist_manager, CollectionModel *collection_model, QObject *parent = nullptr);

  // DO NOT CHANGE - values get saved in Settings
  enum class Mode {
    Off = 0,
    Track = 1,
    Album = 2,
    Artist = 3
  };

  Mode mode() const { return mode_; }
  void CycleMode();

 public Q_SLOTS:
  void PlaylistDone();

 private:
  const SharedPtr<PlaylistManager> playlist_manager_;
  CollectionModel *collection_model_;
  Mode mode_;
};

#endif  // COLLECTIONAUTOQUEUE_H
