/*
 * Strawberry Music Player
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

#include <QtGlobal>

#include <memory>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#  include <cstring>
#  include <sys/stat.h>
#endif

#ifdef Q_OS_WIN32
#  include <io.h>
#  include <cstring>
#  include <windows.h>
#endif

#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDateTime>

#include "core/logging.h"
#include "includes/scoped_ptr.h"

#include "fileutils.h"

namespace Utilities {

using std::unique_ptr;

QByteArray ReadDataFromFile(const QString &filename) {

  QFile file(filename);
  QByteArray data;
  if (file.open(QIODevice::ReadOnly)) {
    data = file.readAll();
    file.close();
  }
  else {
    qLog(Error) << "Failed to open file" << filename << "for reading:" << file.errorString();
  }
  return data;

}

bool Copy(QIODevice *source, QIODevice *destination) {

  if (!source->open(QIODevice::ReadOnly)) return false;

  if (!destination->open(QIODevice::WriteOnly)) return false;

  const qint64 bytes = source->size();
  // size() returns -1 for sequential devices (sockets, processes); casting that to size_t would attempt a SIZE_MAX allocation.
  if (bytes < 0) return false;
  unique_ptr<char[]> data(new char[static_cast<size_t>(bytes)]);
  qint64 pos = 0;

  qint64 bytes_read = 0;
  do {
    bytes_read = source->read(data.get() + pos, bytes - pos);
    if (bytes_read == -1) return false;

    pos += bytes_read;
  } while (bytes_read > 0 && pos != bytes);

  // A short read (read() returning 0 before all bytes were read) leaves the buffer partially filled - don't go on to write a truncated/garbage copy and report success.
  if (pos != bytes) return false;

  pos = 0;
  qint64 bytes_written = 0;
  do {
    bytes_written = destination->write(data.get() + pos, bytes - pos);
    if (bytes_written == -1) return false;

    pos += bytes_written;
  } while (bytes_written > 0 && pos != bytes);

  // Likewise a short write means the copy is incomplete despite no -1 error.
  if (pos != bytes) return false;

  return true;

}

bool CopyRecursive(const QString &source, const QString &destination) {

  // Make the destination directory
  QString dir_name = source.section(u'/', -1, -1);
  QString dest_path = destination + QLatin1Char('/') + dir_name;
  QDir().mkpath(dest_path);

  QDir dir(source);
  const QStringList children_dirs = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
  for (const QString &child : children_dirs) {
    if (!CopyRecursive(source + QLatin1Char('/') + child, dest_path)) {
      qLog(Warning) << "Failed to copy dir" << source + QLatin1Char('/') + child << "to" << dest_path;
      return false;
    }
  }

  const QStringList children_files = dir.entryList(QDir::NoDotAndDotDot | QDir::Files);
  for (const QString &child : children_files) {
    if (!QFile::copy(source + QLatin1Char('/') + child, dest_path + QLatin1Char('/') + child)) {
      qLog(Warning) << "Failed to copy file" << source + QLatin1Char('/') + child << "to" << dest_path;
      return false;
    }
  }

  return true;

}

namespace {

bool IsLink(const QFileInfo &fileinfo) {
  return fileinfo.isSymLink() || fileinfo.isJunction();
}

bool RemoveLink(const QString &path) {
  // A link to a directory is a directory on Windows, so it is removed as a directory there.
  return QFile::remove(path) || QDir().rmdir(path);
}

#ifdef Q_OS_WIN32
enum class DirectoryIdResult {
  NotDirectory,
  Unavailable,
  Available,
};

// Gets the volume serial number and the 128-bit file ID of the existing directory, the links in the path are followed.
// The 64-bit file index is not used, it's not unique on ReFS.
DirectoryIdResult GetDirectoryId(const QString &directory, FILE_ID_INFO *file_id_info) {

  // A shortcut is resolved like Qt does for the file information, CreateFileW() would open the shortcut file itself.
  // The target of a broken shortcut is empty, it can't be opened.
  const QFileInfo fileinfo(directory);
  const QString path = fileinfo.isShortcut() ? fileinfo.symLinkTarget() : directory;

  // Opened without access, so it doesn't conflict with other handles, the backup semantics flag is needed to open a directory.
  // When it can't be opened, it's not known whether it's a directory, for example a path longer than MAX_PATH can't be opened without the long path prefix.
  const HANDLE handle = ::CreateFileW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(path).utf16()), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return DirectoryIdResult::Unavailable;

  FILE_BASIC_INFO basic_info {};
  if (::GetFileInformationByHandleEx(handle, FileBasicInfo, &basic_info, sizeof(basic_info)) == 0) {
    ::CloseHandle(handle);
    return DirectoryIdResult::Unavailable;
  }
  if ((basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    ::CloseHandle(handle);
    return DirectoryIdResult::NotDirectory;
  }

  const bool success = ::GetFileInformationByHandleEx(handle, FileIdInfo, file_id_info, sizeof(FILE_ID_INFO)) != 0;
  ::CloseHandle(handle);
  if (!success) return DirectoryIdResult::Unavailable;

  // A file ID of all zeros or all ones is not a valid file ID, so it doesn't identify the directory.
  FILE_ID_128 zero_file_id {};
  FILE_ID_128 invalid_file_id {};
  std::memset(invalid_file_id.Identifier, 0xFF, sizeof(invalid_file_id.Identifier));
  if (std::memcmp(&file_id_info->FileId, &zero_file_id, sizeof(FILE_ID_128)) == 0 || std::memcmp(&file_id_info->FileId, &invalid_file_id, sizeof(FILE_ID_128)) == 0) {
    return DirectoryIdResult::Unavailable;
  }

  return DirectoryIdResult::Available;

}
#endif

}  // namespace

bool IsSameDirectory(const QString &directory1, const QString &directory2) {

#ifdef Q_OS_UNIX
  // Compare the directories by their device and inode, the case of their paths can differ on case-insensitive filesystems, and only differing case can be different directories on case-sensitive filesystems.
  struct stat stat1 {};
  struct stat stat2 {};
  if (::stat(QFile::encodeName(directory1).constData(), &stat1) != 0 || ::stat(QFile::encodeName(directory2).constData(), &stat2) != 0) return false;
  return S_ISDIR(stat1.st_mode) && stat1.st_dev == stat2.st_dev && stat1.st_ino == stat2.st_ino;
#else
#  ifdef Q_OS_WIN32
  // Compare the directories by their volume serial number and file ID, the canonical paths keep the case of the paths and link targets, not the case of the directories on the filesystem.
  FILE_ID_INFO file_id_info1 {};
  FILE_ID_INFO file_id_info2 {};
  const DirectoryIdResult result1 = GetDirectoryId(directory1, &file_id_info1);
  const DirectoryIdResult result2 = GetDirectoryId(directory2, &file_id_info2);
  if (result1 == DirectoryIdResult::NotDirectory || result2 == DirectoryIdResult::NotDirectory) return false;
  if (result1 == DirectoryIdResult::Available && result2 == DirectoryIdResult::Available) {
    return file_id_info1.VolumeSerialNumber == file_id_info2.VolumeSerialNumber && std::memcmp(&file_id_info1.FileId, &file_id_info2.FileId, sizeof(FILE_ID_128)) == 0;
  }
  // When the filesystem has no file IDs, or the directories can't be opened, the canonical paths are compared.
#  endif
  const QFileInfo fileinfo1(directory1);
  const QString canonical_directory1 = fileinfo1.canonicalFilePath();
  return fileinfo1.isDir() && !canonical_directory1.isEmpty() && canonical_directory1 == QFileInfo(directory2).canonicalFilePath();
#endif

}

bool PathIsInsideDirectory(const QString &path, const QString &directory) {

  // The path is resolved, so a link inside the directory to a directory outside it is not inside it, and a link outside the directory to a directory inside it is.
  // Then its parent directories are compared with the directory.
  const QString canonical_path = QFileInfo(path).canonicalFilePath();
  if (canonical_path.isEmpty() || !QFileInfo(directory).isDir()) return false;

  QDir parent_dir(canonical_path);
  while (parent_dir.cdUp()) {
    if (IsSameDirectory(parent_dir.absolutePath(), directory)) return true;
  }

  return false;

}

bool RemoveRecursive(const QString &path) {

  // Links are removed, not followed, otherwise the files in the directory they link to, which is outside this directory, would be removed too.
  if (IsLink(QFileInfo(path))) {
    return RemoveLink(path);
  }

  QDir dir(path);
  // System is needed to list broken links.
  const QFileInfoList children = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
  for (const QFileInfo &child : children) {
    const QString child_path = child.absoluteFilePath();
    bool success = false;
    if (IsLink(child)) {
      success = RemoveLink(child_path);
    }
    else if (child.isDir()) {
      success = RemoveRecursive(child_path);
    }
    else {
      success = QFile::remove(child_path);
    }
    if (!success) {
      return false;
    }
  }

  return dir.rmdir(path);

}

// Whether the path is on a GVFS FUSE mount (GNOME's virtual filesystem, e.g. /run/user/<uid>/gvfs/... or the older ~/.gvfs/...).
// Those mounts expose backends such as sftp, smb and mtp that do not support random, in-place writes; the whole file has to be rewritten instead.  Ordinary FUSE filesystems like sshfs do support in-place writes.
bool FilenameOnGVFS(const QString &filename) {

#ifdef Q_OS_UNIX
  return filename.contains(QLatin1String("/gvfs/")) || filename.contains(QLatin1String("/.gvfs/"));
#else
  Q_UNUSED(filename)
  return false;
#endif  // Q_OS_UNIX

}

// Copies the contents of source over destination (truncating it), streaming in chunks so large files are not held in memory.
// Unlike QFile::copy() this overwrites an existing destination, and it only writes sequentially from the start.
bool CopyFileContents(const QString &source, const QString &destination) {

  QFile source_file(source);
  if (!source_file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Failed to open" << source << "for reading:" << source_file.errorString();
    return false;
  }

  QFile destination_file(destination);
  if (!destination_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qLog(Error) << "Failed to open" << destination << "for writing:" << destination_file.errorString();
    return false;
  }

  return CopyFileContents(&source_file, &destination_file);

}

bool CopyFileContents(QFile *source_file, QFile *destination_file) {

  const QString source = source_file->fileName();
  const QString destination = destination_file->fileName();

  constexpr qint64 kChunkSize = 64 * 1024;
  for (;;) {
    const QByteArray chunk = source_file->read(kChunkSize);
    if (chunk.isEmpty()) break;
    if (destination_file->write(chunk) != chunk.size()) {
      qLog(Error) << "Failed to write to" << destination << ":" << destination_file->errorString();
      return false;
    }
  }

  if (source_file->error() != QFileDevice::NoError) {
    qLog(Error) << "Failed to read from" << source << ":" << source_file->errorString();
    return false;
  }

  // Flush and surface any error from closing (e.g. the remote write failing on commit).
  if (!destination_file->flush()) {
    qLog(Error) << "Failed to flush" << destination << ":" << destination_file->errorString();
    return false;
  }

  // flush() only pushes the data into the OS; force it out to the underlying storage so that deferred write-back failures (common on network/FUSE-backed filesystems such as GVFS) are reported here rather than being silently lost when the file is closed.
  // The destination is always a file in the filesystem, so it has a handle when it's open, on Windows getting the handle can fail, then the copy is not known to be written, and it fails, so the original is not removed when moving.
  const int handle = destination_file->handle();
  if (handle == -1) {
    qLog(Error) << "Failed to get the handle of" << destination << "to write it to storage";
    return false;
  }
#ifdef Q_OS_UNIX
  if (fsync(handle) != 0) {
    qLog(Error) << "Failed to fsync" << destination << "to storage:" << strerror(errno);
    return false;
  }
#endif
#ifdef Q_OS_WIN32
  if (_commit(handle) != 0) {
    qLog(Error) << "Failed to commit" << destination << "to storage";
    return false;
  }
#endif

  return true;

}

qint64 FileMtimeMsec(const QString &filename) {

  if (filename.isEmpty()) return 0;

  const QFileInfo fileinfo(filename);
  if (!fileinfo.exists() || !fileinfo.lastModified().isValid()) return 0;

  return fileinfo.lastModified().toMSecsSinceEpoch();

}

}  // namespace Utilities
