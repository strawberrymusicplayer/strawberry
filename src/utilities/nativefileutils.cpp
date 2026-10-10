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

#include <QtGlobal>

#ifdef Q_OS_MACOS
#  include <algorithm>
#  include <cstring>
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QUrl>

#include "nativefileutils.h"

namespace Utilities {

#ifdef Q_OS_MACOS

QByteArray NativeFilePath(const QString &path) {

  // This is what Qt passes to the system, so wherever Qt can reach the file the result is identical to Qt's.
  const QByteArray default_path = QFile::encodeName(path);

  const bool is_ascii = std::all_of(path.cbegin(), path.cend(), [](const QChar c) { return c.unicode() < 128; });
  if (is_ascii || !path.startsWith(u'/')) {
    return default_path;
  }

  struct stat st;
  if (::lstat(default_path.constData(), &st) == 0) {
    return default_path;
  }

  // Each path component can be stored in a different normalization form, so resolve them one at a time.
  QByteArray resolved_path;
  const QStringList components = path.split(u'/', Qt::SkipEmptyParts);
  for (const QString &component : components) {
    const QByteArray component_nfd = component.normalized(QString::NormalizationForm_D).toUtf8();
    const QByteArray component_nfc = component.normalized(QString::NormalizationForm_C).toUtf8();
    if (component_nfd == component_nfc) {
      resolved_path += '/' + component_nfd;
      continue;
    }
    QByteArray candidate_path = resolved_path + '/' + component_nfd;
    if (::lstat(candidate_path.constData(), &st) != 0) {
      candidate_path = resolved_path + '/' + component_nfc;
      if (::lstat(candidate_path.constData(), &st) != 0) {
        return default_path;
      }
    }
    resolved_path = candidate_path;
  }

  return resolved_path;

}

QUrl NativeFileUrl(const QUrl &url) {

  if (!url.isLocalFile() || !url.host().isEmpty()) {
    return url;
  }

  const QString path = url.toLocalFile();
  const QByteArray verbatim_path = path.toUtf8();
  struct stat st;
  if (::lstat(verbatim_path.constData(), &st) == 0) {
    return url;
  }

  const QByteArray native_path = NativeFilePath(path);
  if (native_path == verbatim_path) {
    return url;
  }

  return QUrl::fromLocalFile(QString::fromUtf8(native_path));

}

QStringList ListDirectory(const QString &path) {

  QStringList entries;

  DIR *dir = ::opendir(NativeFilePath(path).constData());
  if (!dir) {
    return entries;
  }

  const int fd = ::dirfd(dir);
  const QString prefix = path.endsWith(u'/') ? path : path + u'/';
  while (const struct dirent *entry = ::readdir(dir)) {
    // Skips "." and ".." too.
    if (entry->d_name[0] == '.') continue;
    // Examine the entry by the name exactly as it is stored, so this works regardless of normalization form.
    struct stat st;
    if (::fstatat(fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) continue;
    if (st.st_flags & UF_HIDDEN) continue;
    if (S_ISLNK(st.st_mode) && ::fstatat(fd, entry->d_name, &st, 0) != 0) continue;
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) continue;
    entries << prefix + QFile::decodeName(entry->d_name);
  }
  ::closedir(dir);

  // Two entries which differ only in normalization form have the same path in Qt's spelling.
  entries.removeDuplicates();

  return entries;

}

bool OpenFileForReading(QFile *file) {

  const QByteArray native_path = NativeFilePath(file->fileName());
  if (native_path == QFile::encodeName(file->fileName())) {
    return file->open(QIODevice::ReadOnly);
  }

  const int fd = ::open(native_path.constData(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    // Let QFile fail, so that the error string is set.
    return file->open(QIODevice::ReadOnly);
  }

  if (!file->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    return false;
  }

  return true;

}

#else

#ifndef Q_OS_WIN32
QByteArray NativeFilePath(const QString &path) {
  return QFile::encodeName(path);
}
#endif

QUrl NativeFileUrl(const QUrl &url) {
  return url;
}

QStringList ListDirectory(const QString &path) {

  QStringList entries;
  QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  while (it.hasNext()) {
    entries << it.next();
  }

  return entries;

}

bool OpenFileForReading(QFile *file) {
  return file->open(QIODevice::ReadOnly);
}

#endif  // Q_OS_MACOS

}  // namespace Utilities

#ifdef Q_OS_MACOS

NativeFileInfo::NativeFileInfo(const QString &path)
    : fileinfo_(path),
      native_path_(Utilities::NativeFilePath(path)),
      have_lstat_(false),
      have_stat_(false) {

  memset(&lstat_, 0, sizeof(lstat_));
  memset(&stat_, 0, sizeof(stat_));

  if (path.isEmpty()) {
    return;
  }

  have_lstat_ = ::lstat(native_path_.constData(), &lstat_) == 0;
  if (have_lstat_ && S_ISLNK(lstat_.st_mode)) {
    // Like QFileInfo, everything except the symbolic link functions describes the target of the link.
    have_stat_ = ::stat(native_path_.constData(), &stat_) == 0;
  }
  else {
    stat_ = lstat_;
    have_stat_ = have_lstat_;
  }

}

bool NativeFileInfo::exists(const QString &path) {
  return NativeFileInfo(path).exists();
}

bool NativeFileInfo::isHidden() const {
  return fileinfo_.fileName().startsWith(u'.') || (have_lstat_ && (lstat_.st_flags & UF_HIDDEN));
}

QString NativeFileInfo::symLinkTarget() const {

  if (!isSymLink()) {
    return QString();
  }

  QByteArray buffer(PATH_MAX, '\0');
  const ssize_t len = ::readlink(native_path_.constData(), buffer.data(), static_cast<size_t>(buffer.size()));
  if (len <= 0) {
    return QString();
  }
  buffer.truncate(len);

  const QString target = QFile::decodeName(buffer);
  if (target.startsWith(u'/')) {
    return QDir::cleanPath(target);
  }

  return QDir::cleanPath(fileinfo_.absolutePath() + u'/' + target);

}

QDateTime NativeFileInfo::lastModified() const {

  if (!have_stat_) {
    return QDateTime();
  }

  return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(stat_.st_mtimespec.tv_sec) * 1000 + stat_.st_mtimespec.tv_nsec / 1000000);

}

QDateTime NativeFileInfo::birthTime() const {

  if (!have_stat_ || stat_.st_birthtimespec.tv_sec <= 0) {
    return QDateTime();
  }

  return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(stat_.st_birthtimespec.tv_sec) * 1000 + stat_.st_birthtimespec.tv_nsec / 1000000);

}

#endif  // Q_OS_MACOS
