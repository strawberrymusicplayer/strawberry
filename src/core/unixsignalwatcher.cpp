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

#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <cerrno>
#include <fcntl.h>

#include <QSocketNotifier>

#include "core/logging.h"
#include "unixsignalwatcher.h"

UnixSignalWatcher *UnixSignalWatcher::sInstance = nullptr;

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent),
      signal_fd_{-1, -1},
      socket_notifier_(nullptr) {

  Q_ASSERT(!sInstance);

  // Create a socket pair for the self-pipe trick
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signal_fd_) != 0) {
    qLog(Error) << "Failed to create socket pair for signal handling:" << ::strerror(errno);
    return;
  }

  // Set the read end to non-blocking mode
  // Non-blocking mode is important to prevent HandleSignalNotification from blocking
  int flags = ::fcntl(signal_fd_[0], F_GETFL, 0);
  if (flags == -1) {
    qLog(Error) << "Failed to get socket flags:" << ::strerror(errno);
  }
  else if (::fcntl(signal_fd_[0], F_SETFL, flags | O_NONBLOCK) == -1) {
    qLog(Error) << "Failed to set socket to non-blocking:" << ::strerror(errno);
  }

  // Set the write end to non-blocking mode as well (used in signal handler)
  // Non-blocking mode prevents the signal handler from blocking if buffer is full
  flags = ::fcntl(signal_fd_[1], F_GETFL, 0);
  if (flags == -1) {
    qLog(Error) << "Failed to get socket flags for write end:" << ::strerror(errno);
  }
  else if (::fcntl(signal_fd_[1], F_SETFL, flags | O_NONBLOCK) == -1) {
    qLog(Error) << "Failed to set write end of socket to non-blocking:" << ::strerror(errno);
  }

  // Set up QSocketNotifier to monitor the read end of the socket
  // Note: We proceed even if fcntl failed above, as the socket pair is still functional
  // (just in blocking mode). fcntl failures are extremely rare in practice.
  socket_notifier_ = new QSocketNotifier(signal_fd_[0], QSocketNotifier::Read, this);
  QObject::connect(socket_notifier_, &QSocketNotifier::activated, this, &UnixSignalWatcher::HandleSignalNotification);

  // Set the singleton instance only after successful core initialization (socketpair succeeded)
  sInstance = this;

}

UnixSignalWatcher::~UnixSignalWatcher() {
  // Disable socket notifier first to prevent it from triggering
  // after file descriptors are closed or during signal handler restoration
  // The notifier will be automatically deleted by Qt's parent-child ownership
  if (socket_notifier_) {
    socket_notifier_->setEnabled(false);
  }

  // Restore original signal handlers
  for (int i = 0; i < watched_signals_.size(); ++i) {
    if (::sigaction(watched_signals_[i], &original_signal_actions_[i], nullptr) != 0) {
      qLog(Error) << "Failed to restore signal handler for signal" << watched_signals_[i] << ":" << ::strerror(errno);
    }
  }

  if (signal_fd_[0] != -1) {
    ::close(signal_fd_[0]);
    signal_fd_[0] = -1;
  }
  if (signal_fd_[1] != -1) {
    ::close(signal_fd_[1]);
    signal_fd_[1] = -1;
  }

  sInstance = nullptr;
}

void UnixSignalWatcher::WatchForSignal(const int signal) {

  // Check if socket pair was created successfully
  if (signal_fd_[0] == -1 || signal_fd_[1] == -1) {
    qLog(Error) << "Cannot watch for signal: socket pair not initialized";
    return;
  }

  if (watched_signals_.contains(signal)) {
    qLog(Error) << "Already watching for signal" << signal;
    return;
  }

  struct sigaction signal_action{};
  ::memset(&signal_action, 0, sizeof(signal_action));
  sigemptyset(&signal_action.sa_mask);
  signal_action.sa_handler = UnixSignalWatcher::SignalHandler;
  signal_action.sa_flags = SA_RESTART;

  struct sigaction old_signal_action{};
  ::memset(&old_signal_action, 0, sizeof(old_signal_action));
  if (::sigaction(signal, &signal_action, &old_signal_action) != 0) {
    qLog(Error) << "sigaction error:" << ::strerror(errno);
    return;
  }

  watched_signals_ << signal;
  original_signal_actions_ << old_signal_action;

}

void UnixSignalWatcher::SignalHandler(const int signal) {

  // Note: There is a theoretical race condition here if the destructor runs
  // on the main thread while a signal is being delivered. The check may pass
  // but sInstance could be set to nullptr before the write() call executes,
  // which would cause a segmentation fault when accessing signal_fd_[1].
  // In practice, this window is extremely small and unlikely to occur during
  // normal shutdown. This trade-off is acceptable given the constraints of
  // signal handler safety and the typical shutdown sequence. Applications can
  // minimize this risk by ensuring signal processing completes before destroying
  // the UnixSignalWatcher instance during shutdown.
  if (!sInstance || sInstance->signal_fd_[1] == -1) {
    return;
  }

  // Write the signal number to the socket pair (async-signal-safe)
  // This is the only operation we perform in the signal handler
  // Ignore errors as there's nothing we can safely do about them in a signal handler
  (void)::write(sInstance->signal_fd_[1], &signal, sizeof(signal));

}

void UnixSignalWatcher::HandleSignalNotification() {

  // Read all pending signals from the socket
  // Multiple signals could arrive before the notifier triggers
  while (true) {
    int signal = 0;
    const ssize_t bytes_read = ::read(signal_fd_[0], &signal, sizeof(signal));
    if (bytes_read == sizeof(signal)) {
      qLog(Debug) << "Caught signal:" << signal;
      Q_EMIT UnixSignal(signal);
    }
    else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // No more data available (expected with non-blocking socket)
      break;
    }
    else {
      // Error occurred or partial read
      break;
    }
  }

}
