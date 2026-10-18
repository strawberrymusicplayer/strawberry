/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>

#include "core/logging.h"
#include "utilities/fileutils.h"
#include "musicstorage.h"

#include "filesystemmusicstorage.h"

namespace {

bool FilesHaveSameContents(const QString &filename1, const QString &filename2) {

  if (QFileInfo(filename1).size() != QFileInfo(filename2).size()) return false;

  QFile file1(filename1);
  QFile file2(filename2);
  if (!file1.open(QIODevice::ReadOnly) || !file2.open(QIODevice::ReadOnly)) return false;

  return file1.readAll() == file2.readAll();

}

}  // namespace

FilesystemMusicStorage::FilesystemMusicStorage(const Song::Source source, const QString &root, const std::optional<int> collection_directory_id) : source_(source), root_(root), collection_directory_id_(collection_directory_id) {}

bool FilesystemMusicStorage::CopyToStorage(const CopyJob &job, QString &error_text) {

  const QFileInfo src = QFileInfo(job.source_);
  const QFileInfo dest = QFileInfo(root_ + QLatin1Char('/') + job.destination_);
  // The source is a temporary file when the file was transcoded, then the original is removed instead, and the empty directories are the ones of the original.
  const QFileInfo original = QFileInfo(job.original());

  QFileInfo cover_src;
  QFileInfo cover_dest;
  if (job.albumcover_ && !job.cover_source_.isEmpty() && !job.cover_dest_.isEmpty()) {
    cover_src = QFileInfo(job.cover_source_);
    cover_dest = QFileInfo(root_ + QLatin1Char('/') + job.cover_dest_);
  }

  // Don't do anything if the destination is the same as the source
  if (src == dest) return true;

  // The destination is made from the tags, so make sure it doesn't point outside the root directory.
  const QString root_path = QDir::cleanPath(QDir(root_).absolutePath());
  const QString root_prefix = root_path.endsWith(u'/') ? root_path : root_path + u'/';
  const auto is_inside_root = [&root_prefix](const QFileInfo &fileinfo) { return QDir::cleanPath(fileinfo.absoluteFilePath()).startsWith(root_prefix); };
  for (const QFileInfo &fileinfo : {dest, cover_dest}) {
    if (fileinfo.filePath().isEmpty() || is_inside_root(fileinfo)) continue;
    error_text = QObject::tr("Destination %1 is outside of %2.").arg(QDir::cleanPath(fileinfo.absoluteFilePath()), root_path);
    qLog(Error) << error_text;
    return false;
  }

  // Create directories as required
  QDir dir;
  if (!dir.mkpath(dest.absolutePath())) {
    error_text = QObject::tr("Failed to create directory %1.").arg(dest.dir().absolutePath());
    qLog(Error) << error_text;
    return false;
  }

  // The cover is not copied when it's already the cover in the destination directory, otherwise overwriting would remove it.
  if (!cover_src.filePath().isEmpty() && cover_src == cover_dest) {
    cover_src = QFileInfo();
    cover_dest = QFileInfo();
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
      if (!result) {
        error_text = QObject::tr("Could not move file %1 to %2.").arg(src.absoluteFilePath(), dest.absoluteFilePath());
        qLog(Error) << error_text;
      }
    }
    // Remove the original of a transcoded file, unless the transcoded file replaced it.
    if (result && original != src && original != dest && original.exists() && !QFile::remove(original.absoluteFilePath())) {
      result = false;
      error_text = QObject::tr("Could not remove original file %1.").arg(original.absoluteFilePath());
      qLog(Error) << error_text;
    }
    // The cover is copied, not moved, since it's shared with the other songs of the album, or it's the cover in the cache.
    bool cover_copied = false;
    if ((!cover_dest.exists() || job.overwrite_) && !cover_src.filePath().isEmpty() && !cover_dest.filePath().isEmpty()) {
      cover_copied = QFile::copy(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
    }
    // Remove the cover when it was next to the song and the song was the last file in that directory, so the directory can be removed.
    // Only when the cover is in the destination, either copied now, or by a previous song of the album, but not when the destination has another cover with the same filename.
    if (result && !cover_src.filePath().isEmpty() && cover_src.absolutePath() == original.absolutePath() && (cover_copied || FilesHaveSameContents(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath()))) {
      const QStringList remaining_entries = QDir(original.absolutePath()).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
      if (remaining_entries == QStringList() << cover_src.fileName()) {
        QFile::remove(cover_src.absoluteFilePath());
      }
    }
    // Remove the directories which are empty now, up to the collection or device directory of the original.
    // Without a limit, this would also remove the collection directory itself, and empty directories above it like the home directory.
    // The directories are compared with links resolved, so a directory outside the root, which is linked from inside it, is not removed.
    if (result && !job.original_root_.isEmpty()) {
      QDir remove_dir(original.absolutePath(), QString(), QDir::Name, QDir::NoDotAndDotDot);
      while (Utilities::PathIsInsideDirectory(remove_dir.absolutePath(), job.original_root_) && remove_dir.isEmpty()) {
        if (!QDir().rmdir(remove_dir.absolutePath())) break;
        remove_dir.cdUp();
      }
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

  const QString path = job.metadata_.url().toLocalFile();
  const QFileInfo fileInfo(path);

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  if (job.use_trash_ && QFile::supportsMoveToTrash()) {
#else
  if (job.use_trash_) {
#endif
    if (QFile::moveToTrash(path)) {
      return true;
    }
    qLog(Warning) << "Moving file to trash failed for" << path << ", falling back to direct deletion";
  }

  bool success = false;
  if (fileInfo.isDir()) {
    success = Utilities::RemoveRecursive(path);
  }
  else {
    success = QFile::remove(path);
  }

  if (!success) {
    qLog(Error) << "Failed to delete file" << path;
  }

  return success;

}
