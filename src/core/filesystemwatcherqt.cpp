/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QFileSystemWatcher>
#include <QString>

#include "core/logging.h"
#include "filesystemwatcherqt.h"

FileSystemWatcherQt::FileSystemWatcherQt(QObject *parent) : FileSystemWatcherInterface(parent), watcher_(new QFileSystemWatcher(this)) {

  QObject::connect(watcher_, &QFileSystemWatcher::directoryChanged, this, &FileSystemWatcherQt::PathChanged);

}

void FileSystemWatcherQt::AddPaths(const QStringList &paths) {

  const QStringList paths_added = watcher_->addPaths(paths);
  if (paths_added.isEmpty()) {
    qLog(Error) << "Failed to add watch for paths" << paths;
  }

}

void FileSystemWatcherQt::AddPath(const QString &path) {

  if (!watcher_->addPath(path)) {
    qLog(Error) << "Failed to add watch for path" << path;
  }

}

void FileSystemWatcherQt::RemovePath(const QString &path) {

  if (!watcher_->removePath(path)) {
    qLog(Error) << "Failed to remove watch for path" << path;
  }

}

void FileSystemWatcherQt::RemovePaths(const QStringList &paths) {

  const QStringList paths_removed = watcher_->removePaths(paths);
  if (paths_removed.isEmpty()) {
    qLog(Error) << "Failed to remove watch for paths" << paths;
  }

}

void FileSystemWatcherQt::Clear() {

  watcher_->removePaths(watcher_->directories());
  watcher_->removePaths(watcher_->files());

}
