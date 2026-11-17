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

#include <QtGlobal>

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#endif

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "tagreaderfilelocks.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

TagReaderFileLocks::Locker::Locker(TagReaderFileLocks *file_locks, const QString &filename, const Mode mode) : file_locks_(file_locks), keys_(Keys(filename)) {

  // The file can be replaced while waiting for the lock, for example on GVFS, then it has another identity, which can also be another hard link.
  // The keys are found again after locking, when they changed, the file is locked again with the new keys.
  Lock(mode);
  QStringList keys = Keys(filename);
  while (keys != keys_) {
    Unlock();
    keys_ = keys;
    Lock(mode);
    keys = Keys(filename);
  }

}

TagReaderFileLocks::Locker::~Locker() {

  Unlock();

}

void TagReaderFileLocks::Locker::Lock(const Mode mode) {

  {
    QMutexLocker l(&file_locks_->mutex_);
    for (const QString &key : std::as_const(keys_)) {
      FileLock &file_lock = file_locks_->locks_[key];
      if (!file_lock.lock) file_lock.lock = make_shared<QReadWriteLock>();
      ++file_lock.users;
      locks_ << file_lock.lock;
    }
  }

  // The file is locked after the mutex is released, so waiting for the lock of this file doesn't hold back the other files.
  for (const SharedPtr<QReadWriteLock> &lock : std::as_const(locks_)) {
    if (mode == Mode::Write) {
      lock->lockForWrite();
    }
    else {
      lock->lockForRead();
    }
  }

}

void TagReaderFileLocks::Locker::Unlock() {

  for (qsizetype i = locks_.count() - 1; i >= 0; --i) {
    locks_[i]->unlock();
  }
  locks_.clear();

  QMutexLocker l(&file_locks_->mutex_);
  for (const QString &key : std::as_const(keys_)) {
    FileLock &file_lock = file_locks_->locks_[key];
    if (--file_lock.users == 0) {
      file_locks_->locks_.remove(key);
    }
  }

}

qsizetype TagReaderFileLocks::count() const {

  QMutexLocker l(&mutex_);
  return locks_.count();

}

QStringList TagReaderFileLocks::Keys(const QString &filename) {

  QStringList keys;

  // The same file can be given as different paths, so links are resolved, which is fast compared to reading the file.
  // When the file doesn't exist, the links of the directory are resolved, so the path is the same when the file is created through another path, otherwise the path is made absolute and clean.
  const QFileInfo fileinfo(filename);
  QString path = fileinfo.canonicalFilePath();
  if (path.isEmpty()) {
    const QString dir_path = QFileInfo(fileinfo.absolutePath()).canonicalFilePath();
    path = dir_path.isEmpty() ? QDir::cleanPath(fileinfo.absoluteFilePath()) : dir_path + u'/' + fileinfo.fileName();
  }
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  // Paths are usually case-insensitive on Windows and macOS, using the same lock for files which only differ in case on a case-sensitive filesystem is harmless.
  path = path.toLower();
#endif
  keys << u"path:"_s + path;

  // Hard links to the same file have different paths, so the file is also identified by its device and inode, or volume and file index on Windows.
#ifdef Q_OS_UNIX
  struct stat file_stat {};
  if (::stat(QFile::encodeName(filename).constData(), &file_stat) == 0) {
    keys << u"id:%1:%2"_s.arg(file_stat.st_dev).arg(file_stat.st_ino);
  }
#endif
#ifdef Q_OS_WIN
  // Opened without access, which doesn't conflict with the file being open for reading or writing.
  const HANDLE handle = ::CreateFileW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(filename).utf16()), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle != INVALID_HANDLE_VALUE) {
    BY_HANDLE_FILE_INFORMATION file_information {};
    const bool success = ::GetFileInformationByHandle(handle, &file_information) != 0;
    ::CloseHandle(handle);
    if (success) {
      keys << u"id:%1:%2:%3"_s.arg(file_information.dwVolumeSerialNumber).arg(file_information.nFileIndexHigh).arg(file_information.nFileIndexLow);
    }
  }
#endif

  keys.sort();

  return keys;

}
