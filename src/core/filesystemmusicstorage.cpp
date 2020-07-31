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

#include "config.h"

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

  QFileInfo cover_src;
  QFileInfo cover_dest;
  if (job.albumcover_ && !job.cover_source_.isEmpty() && !job.cover_dest_.isEmpty()) {
    cover_src = QFileInfo(job.cover_source_);
    cover_dest = QFileInfo(root_ + "/" + job.cover_dest_);
  }

  // Don't do anything if the destination is the same as the source
  if (src == dest) return true;

  // Create directories as required
  QDir dir;
  if (!dir.mkpath(dest.absolutePath())) {
    qLog(Warning) << "Failed to create directory" << dest.dir().absolutePath();
    return false;
  }

  // Remove the destination file if it exists and we want to overwrite
  if (job.overwrite_) {
    if (dest.exists()) QFile::remove(dest.absoluteFilePath());
    if (!cover_dest.filePath().isEmpty() && cover_dest.exists()) QFile::remove(cover_dest.absoluteFilePath());
  }

  // Copy or move
  bool result(true);
  if (job.remove_original_) {
    if (dest.exists() && !job.overwrite_) {
      result = false;
    }
    else {
      result = QFile::rename(src.absoluteFilePath(), dest.absoluteFilePath());
    }
    if ((!cover_dest.exists() || job.overwrite_) && !cover_src.filePath().isEmpty() && !cover_dest.filePath().isEmpty()) {
      QFile::rename(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
    }
    // Remove empty directories.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
    QDir remove_dir(src.absolutePath(), QString(), QDir::Name, QDir::NoDotAndDotDot);
    while (remove_dir.isEmpty()) {
      if (!QDir().rmdir(remove_dir.absolutePath())) break;
      remove_dir.cdUp();
    }
#endif
  }
  else {
    if (dest.exists() && !job.overwrite_) {
      result = false;
    }
    else {
      result = QFile::copy(src.absoluteFilePath(), dest.absoluteFilePath());
    }
    if ((!cover_dest.exists() || job.overwrite_) && !cover_src.filePath().isEmpty() && !cover_dest.filePath().isEmpty()) {
      QFile::copy(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
    }
  }

  return result;

}

bool FilesystemMusicStorage::DeleteFromStorage(const DeleteJob &job) {

  QString path = job.metadata_.url().toLocalFile();
  QFileInfo fileInfo(path);

  if (fileInfo.isDir())
    return Utilities::RemoveRecursive(path);
  else
    return QFile::remove(path);

}
