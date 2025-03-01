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

#include "config.h"

#include <optional>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include "core/logging.h"
#include "utilities/fileutils.h"
#include "musicstorage.h"

#include "filesystemmusicstorage.h"

FilesystemMusicStorage::FilesystemMusicStorage(const Song::Source source, const QString &root, const std::optional<int> collection_directory_id) : source_(source), root_(root), collection_directory_id_(collection_directory_id) {}

bool FilesystemMusicStorage::CopyToStorage(const CopyJob &job, QString &error_text) {

  const QFileInfo src = QFileInfo(job.source_);
  const QFileInfo dest = QFileInfo(root_ + QLatin1Char('/') + job.destination_);

  QFileInfo cover_src;
  QFileInfo cover_dest;
  if (job.albumcover_ && !job.cover_source_.isEmpty() && !job.cover_dest_.isEmpty()) {
    cover_src = QFileInfo(job.cover_source_);
    cover_dest = QFileInfo(root_ + QLatin1Char('/') + job.cover_dest_);
  }

  // Don't do anything if the destination is the same as the source
  if (src == dest) return true;

  // Create directories as required
  QDir dir;
  if (!dir.mkpath(dest.absolutePath())) {
    error_text = QObject::tr("Failed to create directory %1.").arg(dest.dir().absolutePath());
    qLog(Error) << error_text;
    return false;
  }

  // Remove the destination file if it exists, and we want to overwrite
  if (job.overwrite_) {
    if (dest.exists()) QFile::remove(dest.absoluteFilePath());
    if (!cover_dest.filePath().isEmpty() && cover_dest.exists()) QFile::remove(cover_dest.absoluteFilePath());
  }

  // Copy or move
  bool result = true;
  if (job.remove_original_) {
    if (dest.exists() && !job.overwrite_) {
      result = false;
      error_text = QObject::tr("Destination file %1 exists, but not allowed to overwrite.").arg(dest.absoluteFilePath());
      qLog(Error) << error_text;
    }
    else {
      result = QFile::rename(src.absoluteFilePath(), dest.absoluteFilePath());
    }
    if ((!cover_dest.exists() || job.overwrite_) && !cover_src.filePath().isEmpty() && !cover_dest.filePath().isEmpty()) {
      QFile::rename(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
    }
    // Remove empty directories.
    QDir remove_dir(src.absolutePath(), QString(), QDir::Name, QDir::NoDotAndDotDot);
    while (remove_dir.isEmpty()) {
      if (!QDir().rmdir(remove_dir.absolutePath())) break;
      remove_dir.cdUp();
    }
  }
  else {
    if (dest.exists() && !job.overwrite_) {
      result = false;
      error_text = QObject::tr("Destination file %1 exists, but not allowed to overwrite").arg(dest.absoluteFilePath());
      qLog(Error) << error_text;
    }
    else {
      result = QFile::copy(src.absoluteFilePath(), dest.absoluteFilePath());
      if (!result) {
        error_text = QObject::tr("Could not copy file %1 to %2.").arg(src.absoluteFilePath(), dest.absoluteFilePath());
        qLog(Error) << error_text;
      }
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  if (job.use_trash_ && QFile::supportsMoveToTrash()) {
#else
  if (job.use_trash_) {
#endif
    return QFile::moveToTrash(path);
  }

  if (fileInfo.isDir()) {
    return Utilities::RemoveRecursive(path);
  }

  return QFile::remove(path);

}
