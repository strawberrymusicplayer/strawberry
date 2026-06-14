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

#include <windows.h>
#include <fileapi.h>

#include <utility>

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>

#include "logging.h"
#include "filesystemwatcherwin.h"
#include "filesystemwatcherwinthread.h"

using namespace Qt::StringLiterals;

namespace {

constexpr DWORD kFileNotifyChangeFlags = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;

// Collapses spelling differences (separators, redundant elements, trailing slash and case) so the same directory is only ever watched once on Windows' case-insensitive filesystem.
// Used purely as an internal lookup key; the original path is what gets reported back through PathChanged().
QString CleanNativePath(const QString &path) {

  QString native_path = QDir::toNativeSeparators(QDir::cleanPath(path));
  if (!native_path.endsWith('\\'_L1)) {
    native_path.append('\\'_L1);
  }

  return native_path.toCaseFolded();

}

}  // namespace

FileSystemWatcherWin::FileSystemWatcherWin(QObject *parent) : FileSystemWatcherInterface(parent) {}

FileSystemWatcherWin::~FileSystemWatcherWin() {

  for (FileSystemWatcherWinThread *thread : std::as_const(threads_)) {
    QObject::disconnect(thread, nullptr, this, nullptr);
    thread->Stop();
  }
  for (FileSystemWatcherWinThread *thread : std::as_const(threads_)) {
    thread->wait();
  }
  qDeleteAll(threads_);

}

void FileSystemWatcherWin::AddPaths(const QStringList &paths) {

  for (const QString &path : paths) {

    const QString key = CleanNativePath(path);
    if (thread_from_path_.contains(key)) {
      qLog(Warning) << "Already watching path" << path;
      continue;
    }

    QString native_path = QDir::toNativeSeparators(path);
    if (!native_path.endsWith('\\'_L1)) {
      native_path.append('\\'_L1);
    }
    const HANDLE handle = FindFirstChangeNotification(reinterpret_cast<const wchar_t*>(native_path.utf16()), FALSE, kFileNotifyChangeFlags);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
      qLog(Error) << "Failed to add watch for path" << path << "error" << GetLastError();
      continue;
    }

    // Find a running thread with spare capacity, otherwise start a new one. A thread that has stopped (but isn't cleaned up yet) is skipped, otherwise the watch would never be serviced.
    FileSystemWatcherWinThread *chosen_thread = nullptr;
    for (FileSystemWatcherWinThread *thread : std::as_const(threads_)) {
      if (thread->isRunning() && thread->AddPath(path, handle)) {
        chosen_thread = thread;
        break;
      }
    }
    if (!chosen_thread) {
      chosen_thread = new FileSystemWatcherWinThread;
      QObject::connect(chosen_thread, &FileSystemWatcherWinThread::PathChanged, this, &FileSystemWatcherWin::PathChanged);
      QObject::connect(chosen_thread, &FileSystemWatcherWinThread::WatchDropped, this, &FileSystemWatcherWin::WatchDropped);
      QObject::connect(chosen_thread, &QThread::finished, this, &FileSystemWatcherWin::ThreadFinished);
      if (!chosen_thread->AddPath(path, handle)) {
        qLog(Error) << "Failed to assign watch for path" << path;
        FindCloseChangeNotification(handle);
        delete chosen_thread;
        continue;
      }
      chosen_thread->start();
      threads_.append(chosen_thread);
    }

    thread_from_path_.insert(key, chosen_thread);
    original_path_from_key_.insert(key, path);
  }

}

void FileSystemWatcherWin::AddPath(const QString &path) {

  AddPaths(QStringList() << path);

}

void FileSystemWatcherWin::RemovePaths(const QStringList &paths) {

  for (const QString &path : paths) {

    const QString key = CleanNativePath(path);
    if (!thread_from_path_.contains(key)) {
      continue;
    }

    FileSystemWatcherWinThread *thread = thread_from_path_.take(key);
    if (!thread) {
      qLog(Warning) << "Not watching path" << path;
      continue;
    }

    // Pass the exact path the worker was given in AddPath() so its own bookkeeping matches.
    thread->RemovePath(original_path_from_key_.take(key));

    if (thread->IsEmpty()) {
      // Disconnect before tearing down so the finished()/WatchDropped() slots can't fire for a thread we're about to delete.
      QObject::disconnect(thread, nullptr, this, nullptr);
      thread->Stop();
      thread->wait();
      threads_.removeAll(thread);
      thread->deleteLater();
    }

  }

}

void FileSystemWatcherWin::RemovePath(const QString &path) {

  RemovePaths(QStringList() << path);

}

void FileSystemWatcherWin::Clear() {

  RemovePaths(original_path_from_key_.values());

}

void FileSystemWatcherWin::WatchDropped(const QString &path) {

  FileSystemWatcherWinThread *thread = qobject_cast<FileSystemWatcherWinThread*>(sender());

  const QString key = CleanNativePath(path);

  // Ignore a stale drop from a worker that no longer owns this key: a replacement worker may have taken the path over (e.g. via ThreadFinished re-registration) after this queued signal was sent.
  // Use value() rather than operator[] so a missing key isn't inserted.
  if (!thread || thread_from_path_.value(key) != thread) {
    return;
  }

  const QString original_path = original_path_from_key_.value(key, path);
  thread_from_path_.remove(key);
  original_path_from_key_.remove(key);

  // Re-arming the notification failed. If the directory still exists (for example it was replaced), re-establish the watch so monitoring continues; otherwise it is genuinely gone.
  if (QFileInfo::exists(original_path)) {
    AddPath(original_path);
  }

  // Discard the worker if it no longer watches anything (the re-registration above may have reused it, in which case it is no longer empty).
  if (threads_.contains(thread) && thread->IsEmpty()) {
    QObject::disconnect(thread, nullptr, this, nullptr);
    thread->Stop();
    thread->wait();
    threads_.removeAll(thread);
    thread->deleteLater();
  }

}

void FileSystemWatcherWin::ThreadFinished() {

  FileSystemWatcherWinThread *thread = qobject_cast<FileSystemWatcherWinThread*>(sender());
  if (!thread || !threads_.contains(thread)) {
    return;  // Unknown thread, or one already torn down explicitly by RemovePaths()/the destructor.
  }

  // The thread exited unexpectedly (e.g. an unrecoverable wait failure).
  // Re-register every path it was responsible for so monitoring continues.
  QStringList paths_to_restore;
  for (QMap<QString, FileSystemWatcherWinThread*>::iterator it = thread_from_path_.begin(); it != thread_from_path_.end();) {
    if (it.value() == thread) {
      const QString key = it.key();
      if (original_path_from_key_.contains(key)) {
        paths_to_restore.append(original_path_from_key_.take(key));
      }
      it = thread_from_path_.erase(it);
    }
    else {
      ++it;
    }
  }

  threads_.removeAll(thread);
  thread->deleteLater();

  if (!paths_to_restore.isEmpty()) {
    AddPaths(paths_to_restore);
  }

}
