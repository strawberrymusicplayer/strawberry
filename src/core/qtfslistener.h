/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef QTFSLISTENER_H
#define QTFSLISTENER_H

#include "config.h"

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>

#include "filesystemwatcherinterface.h"

class QtFSListener : public FileSystemWatcherInterface {
  Q_OBJECT

 public:
  explicit QtFSListener(QObject *parent);
  void AddPath(const QString &path) override;
  void RemovePath(const QString &path) override;
  void Clear() override;

 private:
  QFileSystemWatcher watcher_;

};

#endif
