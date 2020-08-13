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

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "config.h"

#include <QMetaType>
#include <QList>
#include <QString>
#include <QSqlQuery>

struct Directory {
  Directory() : id(-1) {}

  bool operator ==(const Directory& other) const {
    return path == other.path && id == other.id;
  }

  QString path;
  int id;
};
Q_DECLARE_METATYPE(Directory)

typedef QList<Directory> DirectoryList;
Q_DECLARE_METATYPE(DirectoryList)


struct Subdirectory {
  Subdirectory() : directory_id(-1), mtime(0) {}

  int directory_id;
  QString path;
  qint64 mtime;
};
Q_DECLARE_METATYPE(Subdirectory)

typedef QList<Subdirectory> SubdirectoryList;
Q_DECLARE_METATYPE(SubdirectoryList)

#endif // DIRECTORY_H

