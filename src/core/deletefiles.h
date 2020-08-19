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

#ifndef DELETEFILES_H
#define DELETEFILES_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QStringList>

#include "song.h"

class QThread;
class TaskManager;
class MusicStorage;

class DeleteFiles : public QObject {
  Q_OBJECT

 public:
  explicit DeleteFiles(TaskManager *task_manager, std::shared_ptr<MusicStorage> storage, const bool use_trash);
  ~DeleteFiles() override;

  static const int kBatchSize;

  void Start(const SongList &songs);
  void Start(const QStringList &filenames);

signals:
  void Finished(const SongList &songs_with_errors);

 private slots:
  void ProcessSomeFiles();

 private:
  QThread *thread_;
  QThread *original_thread_;
  TaskManager *task_manager_;
  std::shared_ptr<MusicStorage> storage_;

  SongList songs_;
  bool use_trash_;

  bool started_;

  int task_id_;
  int progress_;

  SongList songs_with_errors_;
};

#endif  // DELETEFILES_H
