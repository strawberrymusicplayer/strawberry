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

namespace {
int signal_fd_[2]{};
}

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent),
      socket_notifier_(nullptr) {

  // Create a socket pair for the self-pipe trick
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signal_fd_) != 0) {
    qLog(Error) << "Failed to create socket pair for signal handling:" << ::strerror(errno);
    return;
  }

  // Set the read end to non-blocking mode
  int flags = ::fcntl(signal_fd_[0], F_GETFL, 0);
  if (flags != -1) {
    ::fcntl(signal_fd_[0], F_SETFL, flags | O_NONBLOCK);
  }

  // Set up QSocketNotifier to monitor the read end of the socket
  socket_notifier_ = new QSocketNotifier(signal_fd_[0], QSocketNotifier::Read, this);
  connect(socket_notifier_, &QSocketNotifier::activated, this, &UnixSignalWatcher::HandleSignalNotification);

}

UnixSignalWatcher::~UnixSignalWatcher() {

  // Restore original signal handlers
  for (int i = 0; i < watched_signals_.size(); ++i) {
    ::sigaction(watched_signals_[i], &original_signal_actions_[i], nullptr);
  }

  if (signal_fd_[0] != -1) {
    ::close(signal_fd_[0]);
    signal_fd_[0] = -1;
  }
  if (signal_fd_[1] != -1) {
    ::close(signal_fd_[1]);
    signal_fd_[1] = -1;
  }

}

void UnixSignalWatcher::WatchForSignal(const int signal) {

  // Check if socket pair was created successfully
  if (signal_fd_[0] == -1 || signal_fd_[1] == -1) {
    qLog(Error) << "Cannot watch for signal: socket pair not initialized";
    return;
  }

  if (watched_signals_.contains(signal)) {
    qLog(Debug) << "Already watching for signal" << signal;
    return;
  }

  struct sigaction signal_action{};
  sigemptyset(&signal_action.sa_mask);
  signal_action.sa_handler = UnixSignalWatcher::SignalHandler;
  signal_action.sa_flags = SA_RESTART;
  
  struct sigaction old_action{};
  if (::sigaction(signal, &signal_action, &old_action) != 0) {
    qLog(Error) << "sigaction error:" << ::strerror(errno);
    return;
  }

  watched_signals_ << signal;
  original_signal_actions_ << old_action;

}

void UnixSignalWatcher::SignalHandler(const int signal) {

  if (signal_fd_[1] == -1) {
    return;
  }

  // Write the signal number to the socket pair (async-signal-safe)
  // This is the only operation we perform in the signal handler
  // Ignore errors as there's nothing we can safely do about them in a signal handler
  (void)::write(signal_fd_[1], &signal, sizeof(signal));

}

void UnixSignalWatcher::HandleSignalNotification() {

  // Read all pending signals from the socket
  // Multiple signals could arrive before the notifier triggers
  while (true) {
    int signal = 0;
    const ssize_t bytes_read = ::read(signal_fd_[0], &signal, sizeof(signal));
    if (bytes_read == sizeof(signal)) {
      qLog(Debug) << "Caught signal:" << ::strsignal(signal);
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
