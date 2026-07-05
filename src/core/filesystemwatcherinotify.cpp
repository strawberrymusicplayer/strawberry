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

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>

#include <cerrno>
#include <cstring>
#include <vector>

#include <QFile>
#include <QFileInfo>

#include "logging.h"
#include "filesystemwatcherinotify.h"

FileSystemWatcherInotify::FileSystemWatcherInotify(QObject *parent)
    : FileSystemWatcherInterface(parent),
      inotify_fd_(-1),
      socket_notifier_(nullptr) {

  inotify_fd_ = ::inotify_init1(IN_CLOEXEC);
  if (inotify_fd_ == -1) {
    qLog(Error) << "Failed to initialize inotify:" << strerror(errno);
    return;
  }

  socket_notifier_ = new QSocketNotifier(inotify_fd_, QSocketNotifier::Read, this);
  QObject::connect(socket_notifier_, &QSocketNotifier::activated, this, &FileSystemWatcherInotify::InotifyRead);

}

FileSystemWatcherInotify::~FileSystemWatcherInotify() {

  if (socket_notifier_) {
    socket_notifier_->setEnabled(false);
  }

  Clear();

  if (inotify_fd_ != -1) {
    ::close(inotify_fd_);
  }

}

void FileSystemWatcherInotify::AddPaths(const QStringList &paths) {

  if (inotify_fd_ == -1) {
    qLog(Error) << "inotify not initialized";
    return;
  }

  for (const QString &path : paths) {
    if (wd_from_path_.contains(path)) {
      qLog(Warning) << "Already watching path" << path;
      continue;
    }
    const QByteArray encoded_path = QFile::encodeName(path);
    const int result = ::inotify_add_watch(inotify_fd_, encoded_path.constData(), (IN_CREATE | IN_ATTRIB | IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE | IN_MOVE_SELF | IN_DELETE | IN_DELETE_SELF));
    if (result == -1) {
      qLog(Error) << "Failed to add inotify watch for path" << path << strerror(errno);
      continue;
    }
    path_from_wd_.insert(result, path);
    wd_from_path_.insert(path, result);
  }

}

void FileSystemWatcherInotify::AddPath(const QString &path) {

  AddPaths(QStringList() << path);

}

void FileSystemWatcherInotify::RemovePaths(const QStringList &paths) {

  if (inotify_fd_ == -1) {
    qLog(Error) << "inotify not initialized";
    return;
  }

  for (const QString &path : paths) {
    if (!wd_from_path_.contains(path)) {
      continue;
    }
    const int wd = wd_from_path_.value(path);
    const int result = ::inotify_rm_watch(inotify_fd_, wd);
    // EINVAL means the kernel already removed the watch (e.g. the directory was deleted or moved); that is not an error and we must still drop our mapping. Only skip the cleanup for genuine failures.
    if (result == -1 && errno != EINVAL) {
      qLog(Error) << "Failed to remove inotify watch for watch descriptor" << wd << "path" << path << strerror(errno);
      continue;
    }
    path_from_wd_.remove(wd);
    wd_from_path_.remove(path);
  }

}

void FileSystemWatcherInotify::RemovePath(const QString &path) {

  RemovePaths(QStringList() << path);

}

void FileSystemWatcherInotify::Clear() {

  RemovePaths(path_from_wd_.values());

}

void FileSystemWatcherInotify::InotifyRead() {

  int input_buffer_size = 0;
  const int ioctl_result = ::ioctl(inotify_fd_, FIONREAD, &input_buffer_size);
  if (ioctl_result == -1) {
    qLog(Error) << "ioctl() failed:" << strerror(errno);
    return;
  }

  if (input_buffer_size == 0) {
    return;
  }

  std::vector<char> buffer(static_cast<size_t>(input_buffer_size));
  const ssize_t read_buffer_size = ::read(inotify_fd_, buffer.data(), input_buffer_size);
  if (read_buffer_size <= 0) {
    if (read_buffer_size == -1) {
      qLog(Error) << "read() failed:" << strerror(errno);
    }
    return;
  }

  char *pos = buffer.data();
  char *const end = pos + read_buffer_size;

  QMap<int, inotify_event*> events;
  while (pos < end) {
    inotify_event *event = reinterpret_cast<inotify_event*>(pos);
    if (events.contains(event->wd)) {
      events[event->wd]->mask |= event->mask;
    }
    else {
      events.insert(event->wd, event);
    }
    pos += sizeof(inotify_event) + event->len;
  }

  for (QMap<int, inotify_event*>::const_iterator it = events.constBegin(); it != events.constEnd(); ++it) {
    const inotify_event &event = **it;
    const int wd = event.wd;
    if (!path_from_wd_.contains(wd)) {
      // No path is associated with this descriptor.
      // This is expected for IN_IGNORED, which the kernel delivers when a watch is removed either by our own inotify_rm_watch() below (in response to IN_MOVE_SELF / IN_DELETE_SELF),
      // or automatically when the watched directory is deleted or moved away by which point we have already dropped the mapping.
      continue;
    }
    const QString path = path_from_wd_.value(wd);

    const bool ignored = (event.mask & IN_IGNORED) != 0;
    const bool self_gone = (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT)) != 0;
    if (ignored || self_gone) {
      // The watch is going away. IN_IGNORED means the kernel has already removed the descriptor, so calling inotify_rm_watch() for it would just fail with EINVAL; only remove it ourselves for move/delete/unmount.
      if (!ignored) {
        const int result = ::inotify_rm_watch(inotify_fd_, wd);
        // EINVAL is expected when the kernel already removed the watch automatically (e.g. the directory was deleted).
        if (result == -1 && errno != EINVAL) {
          qLog(Error) << "Failed to remove inotify watch for watch descriptor" << wd << "path" << path << strerror(errno);
        }
      }
      path_from_wd_.remove(wd);
      wd_from_path_.remove(path);
    }

    // Report the change, except for a bare IN_IGNORED which only signals that the watch went away.
    if (!ignored || self_gone) {
      Q_EMIT PathChanged(path);
    }
  }

}
