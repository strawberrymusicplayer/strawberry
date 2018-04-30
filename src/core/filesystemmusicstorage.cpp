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

#include "config.h"

#include <stdbool.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "core/logging.h"
#include "utilities.h"
#include "song.h"
#include "musicstorage.h"

#include "filesystemmusicstorage.h"

FilesystemMusicStorage::FilesystemMusicStorage(const QString &root)
    : root_(root) {}

bool FilesystemMusicStorage::CopyToStorage(const CopyJob &job) {

  const QFileInfo src = QFileInfo(job.source_);
  const QFileInfo dest = QFileInfo(root_ + "/" + job.destination_);

  // Don't do anything if the destination is the same as the source
  if (src == dest) return true;

  // Create directories as required
  QDir dir;
  if (!dir.mkpath(dest.absolutePath())) {
    qLog(Warning) << "Failed to create directory" << dest.dir().absolutePath();
    return false;
  }

  // Remove the destination file if it exists and we want to overwrite
  if (job.overwrite_ && dest.exists()) QFile::remove(dest.absoluteFilePath());

  // Copy or move
  if (job.remove_original_)
    return QFile::rename(src.absoluteFilePath(), dest.absoluteFilePath());
  else
    return QFile::copy(src.absoluteFilePath(), dest.absoluteFilePath());

}

bool FilesystemMusicStorage::DeleteFromStorage(const DeleteJob &job) {

  QString path = job.metadata_.url().toLocalFile();
  QFileInfo fileInfo(path);

  if (fileInfo.isDir())
    return Utilities::RemoveRecursive(path);
  else
    return QFile::remove(path);

}

