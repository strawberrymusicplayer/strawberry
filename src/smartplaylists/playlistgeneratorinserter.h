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

#ifndef PLAYLISTGENERATORINSERTER_H
#define PLAYLISTGENERATORINSERTER_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"

#include "playlist/playlist.h"
#include "playlist/playlistitem.h"

#include "playlistgenerator_fwd.h"

class TaskManager;
class CollectionBackend;
class Playlist;

class PlaylistGeneratorInserter : public QObject {
  Q_OBJECT

 public:
  explicit PlaylistGeneratorInserter(SharedPtr<TaskManager> task_manager, SharedPtr<CollectionBackend> collection_backend, QObject *parent);

  void Load(Playlist *destination, const int row, const bool play_now, const bool enqueue, const bool enqueue_next, PlaylistGeneratorPtr generator, const int dynamic_count = 0);

 private:
  static PlaylistItemPtrList Generate(PlaylistGeneratorPtr generator, const int dynamic_count);

 Q_SIGNALS:
  void Error(const QString &message);
  void PlayRequested(const QModelIndex idx, const Playlist::AutoScroll autoscroll);

 private Q_SLOTS:
  void Finished();

 private:
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<CollectionBackend> collection_backend_;
  int task_id_;

  Playlist *destination_;
  int row_;
  bool play_now_;
  bool enqueue_;
  bool enqueue_next_;
  bool is_dynamic_;

};

#endif  // PLAYLISTGENERATORINSERTER_H
