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

#include <QMutexLocker>
#include <QList>
#include <QString>

#include "filesystemwatcherwinthread.h"

FileSystemWatcherWinThread::FileSystemWatcherWinThread() : msg_(0) {

  handles_.reserve(MAXIMUM_WAIT_OBJECTS);
  // Auto-reset, initially non-signaled event used to wake the thread when the handle list changes or the thread should quit.
  handles_.append(CreateEvent(nullptr, FALSE, FALSE, nullptr));

}

FileSystemWatcherWinThread::~FileSystemWatcherWinThread() {

  // The thread has stopped by now, so no locking is required.
  CloseHandle(handles_.at(0));
  for (int i = 1; i < handles_.count(); ++i) {
    FindCloseChangeNotification(handles_.at(i));
  }
  for (HANDLE handle : std::as_const(pending_close_)) {
    FindCloseChangeNotification(handle);
  }

}

bool FileSystemWatcherWinThread::AddPath(const QString &path, HANDLE handle) {

  QMutexLocker locker(&mutex_);

  if (handles_.count() >= MAXIMUM_WAIT_OBJECTS) {
    return false;
  }

  handles_.append(handle);
  handle_from_path_.insert(path, handle);
  path_from_handle_.insert(handle, path);

  msg_ = '@';
  SetEvent(handles_.at(0));

  return true;

}

bool FileSystemWatcherWinThread::RemovePath(const QString &path) {

  QMutexLocker locker(&mutex_);

  if (!handle_from_path_.contains(path)) {
    return false;
  }

  const HANDLE handle = handle_from_path_.take(path);
  path_from_handle_.remove(handle);
  handles_.removeAll(handle);

  // Don't close the handle here:
  // The worker thread may currently be waiting on a copy of the handle list that still contains it, and closing a handle that is being waited on is undefined.
  // Hand it to the worker, which closes it once it has returned from the wait.
  pending_close_.append(handle);

  msg_ = '@';
  SetEvent(handles_.at(0));

  return true;

}

bool FileSystemWatcherWinThread::IsEmpty() {

  QMutexLocker locker(&mutex_);
  return handles_.count() <= 1;  // Only the wakeup event remains.

}

void FileSystemWatcherWinThread::Stop() {

  QMutexLocker locker(&mutex_);
  msg_ = 'q';
  SetEvent(handles_.at(0));

}

void FileSystemWatcherWinThread::run() {

  QMutexLocker locker(&mutex_);

  while (true) {
    const QList<HANDLE> handles = handles_;
    locker.unlock();

    const DWORD r = WaitForMultipleObjects(static_cast<DWORD>(handles.count()), handles.constData(), FALSE, INFINITE);

    locker.relock();

    // The wait has returned, so we are no longer waiting on any handle: it is now safe to close the handles that RemovePath() queued while we may have been waiting on them.
    ClosePending();

    if (r == WAIT_FAILED || r < WAIT_OBJECT_0 || r >= WAIT_OBJECT_0 + static_cast<DWORD>(handles.count())) {
      break;
    }

    const int index = static_cast<int>(r - WAIT_OBJECT_0);
    if (index == 0) {
      // Wakeup event: the handle list changed or we have been asked to quit.
      const int msg = msg_;
      msg_ = 0;
      if (msg == 'q') return;
      continue;
    }

    const HANDLE handle = handles.at(index);
    if (!path_from_handle_.contains(handle)) {
      continue;  // Removed concurrently.
    }
    const QString path = path_from_handle_.value(handle);

    // Re-arm the notification; if it fails the directory was probably removed, so drop the watch.
    bool dropped = false;
    if (!FindNextChangeNotification(handle)) {
      FindCloseChangeNotification(handle);
      handles_.removeAll(handle);
      path_from_handle_.remove(handle);
      handle_from_path_.remove(path);
      dropped = true;
    }

    locker.unlock();
    // Tell the owner the watch is gone so its bookkeeping doesn't go stale (which would make a later AddPath() look like a duplicate and silently never re-watch the directory).
    if (dropped) {
      Q_EMIT WatchDropped(path);
    }
    Q_EMIT PathChanged(path);
    locker.relock();
  }

}

void FileSystemWatcherWinThread::ClosePending() {

  // Caller must hold mutex_. Only called from run() after the wait has returned, so the worker is guaranteed not to be waiting on any of these handles.
  for (HANDLE handle : std::as_const(pending_close_)) {
    FindCloseChangeNotification(handle);
  }
  pending_close_.clear();

}
