/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILESYSTEMMUSICSTORAGE_H
#define FILESYSTEMMUSICSTORAGE_H

#include "config.h"

#include <QString>

#include "musicstorage.h"

class FilesystemMusicStorage : public virtual MusicStorage {
 public:
  explicit FilesystemMusicStorage(const QString &root);

  QString LocalPath() const override { return root_; }

  bool CopyToStorage(const CopyJob &job) override;
  bool DeleteFromStorage(const DeleteJob &job) override;

 private:
  QString root_;
};

#endif // FILESYSTEMMUSICSTORAGE_H
