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

#include <QtGlobal>

#include <optional>
#include <cstdio>
#include <cerrno>

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QStorageInfo>
#include <QTemporaryFile>

#include "core/logging.h"
#include "utilities/fileutils.h"
#include "musicstorage.h"

#include "filesystemmusicstorage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

bool FilesHaveSameContents(const QString &filename1, const QString &filename2) {

  if (QFileInfo(filename1).size() != QFileInfo(filename2).size()) return false;

  QFile file1(filename1);
  QFile file2(filename2);
  if (!file1.open(QIODevice::ReadOnly) || !file2.open(QIODevice::ReadOnly)) return false;

  return file1.readAll() == file2.readAll();

}

// Renames the file, without copying it when it can't be renamed, like QFile::rename() does, which writes to the destination directly.
bool RenameFile(const QString &source, const QString &destination) {

  // A rename which only changes the case of the filename needs QFile::rename(), which handles case insensitive filesystems, it's in the same directory, so it's not copied.
  if (source.compare(destination, Qt::CaseInsensitive) == 0) {
    return QFile::rename(source, destination);
  }

  // An existing destination is not replaced, also one created by another process at the same time, so the renames are atomic, and fail when the destination exists.
#ifdef Q_OS_WIN
  return ::MoveFileExW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(source).utf16()), reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(destination).utf16()), 0) != 0;
#else
  const QByteArray source_path = QFile::encodeName(source);
  const QByteArray destination_path = QFile::encodeName(destination);
#  if defined(Q_OS_LINUX) && defined(RENAME_NOREPLACE)
  if (::renameat2(AT_FDCWD, source_path.constData(), AT_FDCWD, destination_path.constData(), RENAME_NOREPLACE) == 0) return true;
  // EINVAL when the filesystem doesn't support it.
  if (errno != EINVAL) return false;
#  endif
#  if defined(Q_OS_DARWIN) && defined(RENAME_EXCL)
  if (::renameatx_np(AT_FDCWD, source_path.constData(), AT_FDCWD, destination_path.constData(), RENAME_EXCL) == 0) return true;
  if (errno != ENOTSUP) return false;
#  endif
  // A link fails when the destination exists, and a symbolic link is linked, not the file it links to.
  if (::linkat(AT_FDCWD, source_path.constData(), AT_FDCWD, destination_path.constData(), 0) == 0) {
    if (::unlink(source_path.constData()) != 0) {
      ::unlink(destination_path.constData());
      return false;
    }
    return true;
  }
  // The filesystem doesn't support links, for example some network and FUSE filesystems, then the file is renamed instead of copying it.
  // The destination is created as an empty file first, which fails when anything exists there, also a link, then it's replaced by the rename, so a file created by another process at the same time is never replaced.
  if (errno != EPERM && errno != EOPNOTSUPP && errno != ENOSYS) return false;
  const int destination_fd = ::open(destination_path.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (destination_fd == -1) return false;
  ::close(destination_fd);
  if (::rename(source_path.constData(), destination_path.constData()) != 0) {
    ::unlink(destination_path.constData());
    return false;
  }
  return true;
#endif

}

// Copies the file to a temporary file in the destination directory, which is renamed to the destination when it's complete, so an interrupted copy doesn't leave a partial file at the destination.
// QFile::copy() copies to the destination directly on Windows.
// The temporary file is renamed with RenameFile(), which doesn't replace an existing file, also one created while copying, then the copy fails, and the temporary file is removed.
bool CopyFileStaged(const QString &source, const QString &destination) {

  QFile source_file(source);
  if (!source_file.open(QIODevice::ReadOnly)) return false;

  // The destination filename can be as long as the filesystem allows, so the temporary file has a short name, it's hidden in case it's left when copying is interrupted.
  QTemporaryFile destination_file(QFileInfo(destination).absolutePath() + u"/.strawberry-XXXXXX"_s);
  if (!destination_file.open()) return false;

  // The copy is also written to the storage before it's renamed, so a deferred write error, for example on a network filesystem, fails the copy, and the original is not removed when moving.
  if (!Utilities::CopyFileContents(&source_file, &destination_file)) return false;

  // The permissions are set before the copy is renamed, so it doesn't have other permissions at the destination.
  // Some filesystems, like FAT, exFAT and CIFS, can't set them, then the copy has the permissions of the filesystem, like QFile::copy() does.
  if (!destination_file.setPermissions(source_file.permissions())) {
    qLog(Warning) << "Could not set the permissions of" << destination << destination_file.errorString();
  }

  // QTemporaryFile::rename() renames without checking the destination when the filesystem doesn't support links.
  // Getting the filename creates the temporary file in the directory if it's unnamed.
  const QString temporary_filename = destination_file.fileName();
  destination_file.close();
  if (temporary_filename.isEmpty() || !RenameFile(temporary_filename, destination)) return false;
  destination_file.setAutoRemove(false);

  return true;

}

// QFile::rename() writes the file to the destination itself when the destination is on another filesystem, so when that is interrupted, for example by quitting, a partial file is left at the destination.
// The file is only renamed on the same filesystem, otherwise it's copied to a temporary file which is renamed when it's complete, then the original is removed.
// Returns true if a file can be renamed from one directory to the other, they're in the same mount, and on Unix on the same device, since for example Btrfs subvolumes in the same mount are different devices.
bool CanRename(const QString &source_path, const QString &destination_path) {

  if (QStorageInfo(source_path).rootPath() != QStorageInfo(destination_path).rootPath()) return false;

#ifdef Q_OS_UNIX
  struct stat source_stat {};
  struct stat destination_stat {};
  if (::stat(QFile::encodeName(source_path).constData(), &source_stat) != 0 || ::stat(QFile::encodeName(destination_path).constData(), &destination_stat) != 0) return false;
  if (source_stat.st_dev != destination_stat.st_dev) return false;
#endif

  return true;

}

// The filesystem of the source is the one of its directory, a link is renamed, not replaced with a copy of the file it links to.
// When the file can't be renamed, it's copied.
bool MoveSongFile(const QString &source, const QString &destination) {

  if (CanRename(QFileInfo(source).absolutePath(), QFileInfo(destination).absolutePath()) && RenameFile(source, destination)) {
    return true;
  }

  if (!CopyFileStaged(source, destination)) return false;

  if (!QFile::remove(source)) {
    QFile::remove(destination);
    return false;
  }

  return true;

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
      result = MoveSongFile(src.absoluteFilePath(), dest.absoluteFilePath());
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
      cover_copied = CopyFileStaged(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
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
      result = CopyFileStaged(src.absoluteFilePath(), dest.absoluteFilePath());
      if (!result) {
        error_text = QObject::tr("Could not copy file %1 to %2.").arg(src.absoluteFilePath(), dest.absoluteFilePath());
        qLog(Error) << error_text;
      }
    }
    if ((!cover_dest.exists() || job.overwrite_) && !cover_src.filePath().isEmpty() && !cover_dest.filePath().isEmpty()) {
      CopyFileStaged(cover_src.absoluteFilePath(), cover_dest.absoluteFilePath());
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
