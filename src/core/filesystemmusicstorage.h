/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <optional>

#include <QString>

#include "song.h"
#include "musicstorage.h"

class FilesystemMusicStorage : public virtual MusicStorage {
 public:
  explicit FilesystemMusicStorage(const Song::Source source, const QString &root, const std::optional<int> collection_directory_id = std::optional<int>());

  Song::Source source() const override { return source_; }
  QString LocalPath() const override { return root_; }
  std::optional<int> collection_directory_id() const override { return collection_directory_id_; }

  bool CopyToStorage(const CopyJob &job, QString &error_text) override;
  bool DeleteFromStorage(const DeleteJob &job) override;

 private:
  Song::Source source_;
  QString root_;
  std::optional<int> collection_directory_id_;

  Q_DISABLE_COPY(FilesystemMusicStorage)
};

#endif  // FILESYSTEMMUSICSTORAGE_H
