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

#ifndef FILESYSTEMWATCHERINTERFACE_H
#define FILESYSTEMWATCHERINTERFACE_H

#include <QObject>
#include <QString>
#include <QStringList>

class FileSystemWatcherInterface : public QObject {
  Q_OBJECT

 public:
  explicit FileSystemWatcherInterface(QObject *parent = nullptr);

  virtual void AddPaths(const QStringList &paths) = 0;
  virtual void RemovePaths(const QStringList &paths) = 0;
  virtual void AddPath(const QString &path) = 0;
  virtual void RemovePath(const QString &path) = 0;
  virtual void Clear() = 0;

 Q_SIGNALS:
  void PathChanged(const QString &path);
};

#endif  // FILESYSTEMWATCHERINTERFACE_H
