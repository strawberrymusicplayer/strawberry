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

#ifndef FILESYSTEMWATCHERWIN_H
#define FILESYSTEMWATCHERWIN_H

#include <QList>
#include <QMap>

#include "filesystemwatcherinterface.h"

class FileSystemWatcherWinThread;

class FileSystemWatcherWin : public FileSystemWatcherInterface {
  Q_OBJECT

 public:
  explicit FileSystemWatcherWin(QObject *parent = nullptr);
  ~FileSystemWatcherWin() override;

  void AddPaths(const QStringList &paths) override;
  void RemovePaths(const QStringList &paths) override;
  void AddPath(const QString &path) override;
  void RemovePath(const QString &path) override;
  void Clear() override;

 private Q_SLOTS:
  // A worker dropped a single watch (directory removed); forget the path so it can be re-added.
  void WatchDropped(const QString &path);
  // A worker thread terminated (e.g. an unrecoverable wait failure):
  // Forget all its watches and discard the thread, so its paths can be re-added and its slot isn't picked for new watches.
  void ThreadFinished();

 private:
  QList<FileSystemWatcherWinThread*> threads_;
  QMap<QString, FileSystemWatcherWinThread*> thread_from_path_;
  QMap<QString, QString> original_path_from_key_;
};

#endif  // FILESYSTEMWATCHERWIN_H
