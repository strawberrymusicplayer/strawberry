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

#ifdef Q_OS_UNIX
#  include <sys/statvfs.h>
#endif

#ifdef Q_OS_WIN32
#  include <windows.h>
#endif

#include <QString>
#include <QDir>

#include "diskutils.h"

#ifdef Q_OS_WIN32
#  include "scopedwchararray.h"
#endif

namespace Utilities {

quint64 FileSystemCapacity(const QString &path) {

#if defined(Q_OS_UNIX)
  struct statvfs fs_info {};
  if (statvfs(path.toLocal8Bit().constData(), &fs_info) == 0)
    return static_cast<quint64>(fs_info.f_blocks) * static_cast<quint64>(fs_info.f_bsize);
#elif defined(Q_OS_WIN32)
  _ULARGE_INTEGER ret;
  ScopedWCharArray wchar(QDir::toNativeSeparators(path));
  if (GetDiskFreeSpaceEx(wchar.get(), nullptr, &ret, nullptr) != 0)
    return ret.QuadPart;
#endif

  return 0;

}

quint64 FileSystemFreeSpace(const QString &path) {

#if defined(Q_OS_UNIX)
  struct statvfs fs_info {};
  if (statvfs(path.toLocal8Bit().constData(), &fs_info) == 0)
    return static_cast<quint64>(fs_info.f_bavail) * static_cast<quint64>(fs_info.f_bsize);
#elif defined(Q_OS_WIN32)
  _ULARGE_INTEGER ret;
  ScopedWCharArray wchar(QDir::toNativeSeparators(path));
  if (GetDiskFreeSpaceEx(wchar.get(), &ret, nullptr, nullptr) != 0)
    return ret.QuadPart;
#endif

  return 0;

}

}  // namespace Utilities
