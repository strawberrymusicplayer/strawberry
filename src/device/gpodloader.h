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

#ifndef GPODLOADER_H
#define GPODLOADER_H

#include "config.h"

#include <memory>
#include <gpod/itdb.h>

#include <QObject>
#include <QString>

#include "core/song.h"

class QThread;
class TaskManager;
class CollectionBackend;
class ConnectedDevice;

class GPodLoader : public QObject {
  Q_OBJECT

 public:
  explicit GPodLoader(const QString &mount_point, TaskManager *task_manager, CollectionBackend *backend, std::shared_ptr<ConnectedDevice> device);
  ~GPodLoader();

  void set_music_path_prefix(const QString &prefix) { path_prefix_ = prefix; }
  void set_song_type(Song::FileType type) { type_ = type; }

  void Abort() { abort_ = true; }

 public slots:
  void LoadDatabase();

 signals:
  void Error(const QString &message);
  void TaskStarted(int task_id);
  void LoadFinished(Itdb_iTunesDB *db, bool success);

 private:
  Itdb_iTunesDB *TryLoad();

 private:
  std::shared_ptr<ConnectedDevice> device_;
  QThread *original_thread_;

  QString mount_point_;
  QString path_prefix_;
  Song::FileType type_;
  TaskManager *task_manager_;
  CollectionBackend *backend_;
  bool abort_;

};

#endif  // GPODLOADER_H

