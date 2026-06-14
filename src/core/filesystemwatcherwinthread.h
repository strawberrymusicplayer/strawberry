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

#ifndef FILESYSTEMWATCHERWINTHREAD_H
#define FILESYSTEMWATCHERWINTHREAD_H

#include <windows.h>

#include <QThread>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>

// A worker thread that waits on a batch of change-notification handles. WaitForMultipleObjects can wait on at most MAXIMUM_WAIT_OBJECTS handles,
// so FileSystemWatcherWin spreads the watched directories across as many of these threads as needed.
class FileSystemWatcherWinThread : public QThread {
  Q_OBJECT

 public:
  explicit FileSystemWatcherWinThread();
  ~FileSystemWatcherWinThread() override;

  // All of these are called from the owning FileSystemWatcherWin's thread.
  bool AddPath(const QString &path, HANDLE handle);
  bool RemovePath(const QString &path);
  bool IsEmpty();
  void Stop();

 Q_SIGNALS:
  void PathChanged(const QString &path);
  // Emitted when the worker drops a watch on its own (the directory was removed and re-arming the notification failed), so the owner can forget it and allow it to be watched again.
  void WatchDropped(const QString &path);

 protected:
  void run() override;

 private:
  void ClosePending();

  QMutex mutex_;
  QList<HANDLE> handles_;  // handles_[0] is the wakeup event, the rest are notification handles.
  QMap<QString, HANDLE> handle_from_path_;
  QMap<HANDLE, QString> path_from_handle_;
  QList<HANDLE> pending_close_;  // Notification handles to be closed by the worker thread.
  int msg_;
};

#endif  // FILESYSTEMWATCHERWINTHREAD_H
