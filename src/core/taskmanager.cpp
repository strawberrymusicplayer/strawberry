/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QObject>
#include <QMutex>
#include <QList>
#include <QString>

#include "taskmanager.h"

using namespace Qt::Literals::StringLiterals;

TaskManager::TaskManager(QObject *parent) : QObject(parent), next_task_id_(1) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

}

int TaskManager::StartTask(const QString &name) {

  Task t;
  t.name = name;
  t.progress = 0;
  t.progress_max = 0;
  t.blocks_collection_scans = false;

  {
    QMutexLocker l(&mutex_);
    t.id = next_task_id_++;
    tasks_[t.id] = t;
  }

  Q_EMIT TasksChanged();
  return t.id;

}

QList<TaskManager::Task> TaskManager::GetTasks() {

  QList<TaskManager::Task> ret;

  {
    QMutexLocker l(&mutex_);
    ret = tasks_.values();
  }

  return ret;

}

void TaskManager::SetTaskBlocksCollectionScans(const int id) {

  {
    QMutexLocker l(&mutex_);
    if (!tasks_.contains(id)) return;

    tasks_[id].blocks_collection_scans = true;
  }

  Q_EMIT TasksChanged();
  Q_EMIT PauseCollectionWatchers();

}

void TaskManager::SetTaskProgress(const int id, const quint64 progress, const quint64 max) {

  {
    QMutexLocker l(&mutex_);
    if (!tasks_.contains(id)) return;

    Task t = tasks_.value(id);
    t.progress = progress;
    if (max > 0) t.progress_max = max;
    tasks_[id] = t;
  }

  Q_EMIT TasksChanged();
}

void TaskManager::IncreaseTaskProgress(const int id, const quint64 progress, const quint64 max) {

  {
    QMutexLocker l(&mutex_);
    if (!tasks_.contains(id)) return;

    Task t = tasks_.value(id);
    t.progress += progress;
    if (max > 0) t.progress_max = max;
    tasks_[id] = t;
  }

  Q_EMIT TasksChanged();

}

void TaskManager::SetTaskFinished(const int id) {

  bool resume_collection_watchers = false;

  {
    QMutexLocker l(&mutex_);
    if (!tasks_.contains(id)) return;

    if (tasks_.value(id).blocks_collection_scans) {
      resume_collection_watchers = true;
      QList<Task> tasks = tasks_.values();

      if (std::any_of(tasks.begin(), tasks.end(), [id](const Task &task) { return task.id != id && task.blocks_collection_scans; })) {
        resume_collection_watchers = false;
      }

    }

    tasks_.remove(id);
  }

  Q_EMIT TasksChanged();
  if (resume_collection_watchers) Q_EMIT ResumeCollectionWatchers();

}

quint64 TaskManager::GetTaskProgress(int id) {

  {
    QMutexLocker l(&mutex_);
    if (!tasks_.contains(id)) return 0;
    return tasks_.value(id).progress;
  }

}
