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

#ifndef FILESYSTEMWATCHERQT_H
#define FILESYSTEMWATCHERQT_H

#include <QString>

#include "filesystemwatcherinterface.h"

class QFileSystemWatcher;

class FileSystemWatcherQt : public FileSystemWatcherInterface {
  Q_OBJECT

 public:
  explicit FileSystemWatcherQt(QObject *parent = nullptr);

  void AddPaths(const QStringList &paths) override;
  void RemovePaths(const QStringList &paths) override;
  void AddPath(const QString &path) override;
  void RemovePath(const QString &path) override;
  void Clear() override;

 private:
  QFileSystemWatcher *watcher_;
};

#endif  // FILESYSTEMWATCHERQT_H
