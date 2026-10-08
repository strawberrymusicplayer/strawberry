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

#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QtGlobal>
#include <QString>

class QIODevice;

namespace Utilities {

QByteArray ReadDataFromFile(const QString &filename);
bool Copy(QIODevice *source, QIODevice *destination);
bool CopyRecursive(const QString &source, const QString &destination);
bool RemoveRecursive(const QString &path);
bool FilenameOnGVFS(const QString &filename);
bool CopyFileContents(const QString &source, const QString &destination);

// Returns the modification time of the file in milliseconds, 0 if it does not exist.
// Milliseconds rather than seconds, so changes within the same second can be detected on filesystems with a finer timestamp resolution.
qint64 FileMtimeMsec(const QString &filename);

}  // namespace Utilities

#endif  // FILEUTILS_H
