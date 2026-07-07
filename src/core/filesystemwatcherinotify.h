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

#ifndef FILESYSTEMWATCHERINOTIFY_H
#define FILESYSTEMWATCHERINOTIFY_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QSocketNotifier>

#include "filesystemwatcherinterface.h"

class FileSystemWatcherInotify : public FileSystemWatcherInterface {
  Q_OBJECT

 public:
  explicit FileSystemWatcherInotify(QObject *parent = nullptr);
  ~FileSystemWatcherInotify() override;

  void AddPaths(const QStringList &paths) override;
  void RemovePaths(const QStringList &paths) override;
  void AddPath(const QString &path) override;
  void RemovePath(const QString &path) override;
  void Clear() override;

 private Q_SLOTS:
  void InotifyRead();

 private:
  int inotify_fd_;
  QSocketNotifier *socket_notifier_;
  QMap<QString, int> wd_from_path_;
  QMap<int, QString> path_from_wd_;
};

#endif  // FILESYSTEMWATCHERINOTIFY_H
