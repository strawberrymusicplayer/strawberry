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

#include "config.h"

#include <QObject>
#include <QString>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"

#include "playlist/playlist.h"
#include "playlistgenerator.h"
#include "playlistgeneratorinserter.h"

class CollectionBackend;

PlaylistGeneratorInserter::PlaylistGeneratorInserter(SharedPtr<TaskManager> task_manager, SharedPtr<CollectionBackend> collection_backend, QObject *parent)
    : QObject(parent),
      task_manager_(task_manager),
      collection_backend_(collection_backend),
      task_id_(-1),
      destination_(nullptr),
      row_(0),
      play_now_(false),
      enqueue_(false),
      enqueue_next_(false),
      is_dynamic_(false) {}

PlaylistItemPtrList PlaylistGeneratorInserter::Generate(PlaylistGeneratorPtr generator, int dynamic_count) {

  if (dynamic_count > 0) {
    return generator->GenerateMore(dynamic_count);
  }

  return generator->Generate();

}

void PlaylistGeneratorInserter::Load(Playlist *destination, const int row, const bool play_now, const bool enqueue, const bool enqueue_next, PlaylistGeneratorPtr generator, const int dynamic_count) {

  task_id_ = task_manager_->StartTask(tr("Loading smart playlist"));

  destination_ = destination;
  row_ = row;
  play_now_ = play_now;
  enqueue_ = enqueue;
  enqueue_next_ = enqueue_next;
  is_dynamic_ = generator->is_dynamic();

  QObject::connect(&*generator, &PlaylistGenerator::Error, this, &PlaylistGeneratorInserter::Error);

  QFuture<PlaylistItemPtrList> future = QtConcurrent::run(PlaylistGeneratorInserter::Generate, generator, dynamic_count);
  QFutureWatcher<PlaylistItemPtrList> *watcher = new QFutureWatcher<PlaylistItemPtrList>();
  QObject::connect(watcher, &QFutureWatcher<PlaylistItemPtrList>::finished, this, &PlaylistGeneratorInserter::Finished);
  watcher->setFuture(future);

}

void PlaylistGeneratorInserter::Finished() {

  QFutureWatcher<PlaylistItemPtrList> *watcher = static_cast<QFutureWatcher<PlaylistItemPtrList>*>(sender());
  PlaylistItemPtrList items = watcher->result();
  watcher->deleteLater();

  if (items.isEmpty()) {
    if (is_dynamic_) {
      destination_->TurnOffDynamicPlaylist();
    }
  }
  else {
    destination_->InsertItems(items, row_, play_now_, enqueue_);
  }

  task_manager_->SetTaskFinished(task_id_);

  deleteLater();

}
