/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERFILELOCKS_H
#define TAGREADERFILELOCKS_H

#include <QtGlobal>
#include <QList>
#include <QHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"

// Read/write locks of files, so a file is not read while it's written, or written by two threads at once, while other files can be read and written at the same time.
// The lock of a file is only kept while it's used.
// A file is locked by its path, and by its identity when it exists, so the same file given as other paths, also as hard links, uses the same lock.
// The path is also locked, since the file can be replaced while it's written, for example on GVFS, then the file at the path has another identity.
class TagReaderFileLocks {
 public:
  TagReaderFileLocks() = default;

  enum class Mode {
    Read,
    Write,
  };

  // Locks the file while it exists.
  class Locker {
   public:
    explicit Locker(TagReaderFileLocks *file_locks, const QString &filename, const Mode mode);
    ~Locker();

   private:
    Q_DISABLE_COPY_MOVE(Locker)

    void Lock(const Mode mode);
    void Unlock();

    TagReaderFileLocks *file_locks_;
    QStringList keys_;
    QList<SharedPtr<QReadWriteLock>> locks_;
  };

  // Returns the number of locks, of the paths and identities of the files.
  qsizetype count() const;

 private:
  Q_DISABLE_COPY_MOVE(TagReaderFileLocks)

  // Returns the keys of the locks of the file, sorted, so they're always locked in the same order, which can't deadlock.
  static QStringList Keys(const QString &filename);

  struct FileLock {
    SharedPtr<QReadWriteLock> lock;
    int users = 0;
  };

  mutable QMutex mutex_;
  QHash<QString, FileLock> locks_;
};

#endif  // TAGREADERFILELOCKS_H
