/*
 * Strawberry Music Player
 * Copyright 2026, Leigh Smith
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

#ifndef NATIVEFILEUTILS_H
#define NATIVEFILEUTILS_H

#include <QtGlobal>

#ifdef Q_OS_MACOS
#  include <sys/stat.h>
#endif

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QFileInfo>
#include <QUrl>

class QFile;

// On macOS Qt converts every filename it reads to Unicode NFC and every filename it passes to the system to NFD (see QFile::encodeName() / QFile::decodeName()).
// That is harmless on APFS and HFS+, which ignore normalization when looking up names, but on filesystems that compare names byte for byte (NFS and some other network or FUSE mounts)
// a file stored under its NFC name is listed by Qt and then can't be opened or stat'ed through any Qt file API.
// The helpers below keep paths in Qt's spelling everywhere, and find the spelling that actually exists on disk at the point where the filesystem is accessed.
// On other platforms they are plain wrappers around the Qt equivalents.

namespace Utilities {

#ifndef Q_OS_WIN32
// Returns the path encoded for use with POSIX file functions, in the spelling under which it exists on disk.
// If the path does not exist in any spelling, this is the same as QFile::encodeName().
QByteArray NativeFilePath(const QString &path);
#endif

// Returns the URL of a local file in the spelling under which it exists on disk, for consumers that pass the URL to the system verbatim (GStreamer).
// URLs that already work verbatim, and URLs which aren't local files, are returned unchanged.
QUrl NativeFileUrl(const QUrl &url);

// Returns the full paths of the files and directories in a directory, excluding hidden entries, like QDirIterator with QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot.
QStringList ListDirectory(const QString &path);

// Opens a file read-only, like QFile::open().
bool OpenFileForReading(QFile *file);

}  // namespace Utilities

#ifdef Q_OS_MACOS

// The subset of QFileInfo used to examine files on disk, working on the spelling under which the file exists on disk.
class NativeFileInfo {
 public:
  explicit NativeFileInfo(const QString &path);

  static bool exists(const QString &path);

  bool exists() const { return have_stat_; }
  bool isFile() const { return have_stat_ && S_ISREG(stat_.st_mode); }
  bool isDir() const { return have_stat_ && S_ISDIR(stat_.st_mode); }
  bool isSymLink() const { return have_lstat_ && S_ISLNK(lstat_.st_mode); }
  bool isSymbolicLink() const { return isSymLink(); }
  bool isHidden() const;
  QString symLinkTarget() const;
  qint64 size() const { return have_stat_ ? static_cast<qint64>(stat_.st_size) : 0; }
  QDateTime lastModified() const;
  QDateTime birthTime() const;

  QString fileName() const { return fileinfo_.fileName(); }
  QString baseName() const { return fileinfo_.baseName(); }
  QString suffix() const { return fileinfo_.suffix(); }

 private:
  QFileInfo fileinfo_;
  QByteArray native_path_;
  struct stat lstat_;
  struct stat stat_;
  bool have_lstat_;
  bool have_stat_;
};

#else

using NativeFileInfo = QFileInfo;

#endif  // Q_OS_MACOS

#endif  // NATIVEFILEUTILS_H
