/*
 * Strawberry Music Player
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

#include <QtGlobal>

#include <memory>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#  include <cstring>
#endif

#ifdef Q_OS_WIN32
#  include <io.h>
#endif

#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QDir>
#include <QFile>

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

bool RemoveRecursive(const QString &path) {

  QDir dir(path);
  const QStringList children_dirs = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Hidden);
  for (const QString &child : children_dirs) {
    if (!RemoveRecursive(path + QLatin1Char('/') + child)) {
      return false;
    }
  }

  const QStringList children_files = dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);
  for (const QString &child : children_files) {
    if (!QFile::remove(path + QLatin1Char('/') + child)) {
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

  constexpr qint64 kChunkSize = 64 * 1024;
  for (;;) {
    const QByteArray chunk = source_file.read(kChunkSize);
    if (chunk.isEmpty()) break;
    if (destination_file.write(chunk) != chunk.size()) {
      qLog(Error) << "Failed to write to" << destination << ":" << destination_file.errorString();
      return false;
    }
  }

  if (source_file.error() != QFileDevice::NoError) {
    qLog(Error) << "Failed to read from" << source << ":" << source_file.errorString();
    return false;
  }

  // Flush and surface any error from closing (e.g. the remote write failing on commit).
  if (!destination_file.flush()) {
    qLog(Error) << "Failed to flush" << destination << ":" << destination_file.errorString();
    return false;
  }

  // flush() only pushes the data into the OS; force it out to the underlying storage so that deferred write-back failures (common on network/FUSE-backed filesystems such as GVFS) are reported here rather than being silently lost when the file is closed.
  const int handle = destination_file.handle();
  if (handle != -1) {
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
  }

  return true;

}

}  // namespace Utilities
