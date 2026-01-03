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

#include <csignal>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

#include <QSocketNotifier>

#include "core/logging.h"
#include "unixsignalwatcher.h"

int UnixSignalWatcher::signal_fd_[2] = {-1, -1};

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent),
      socket_notifier_(nullptr) {

  // Create a socket pair for the self-pipe trick
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signal_fd_) != 0) {
    qLog(Error) << "Failed to create socket pair for signal handling:" << ::strerror(errno);
    return;
  }

  // Set up QSocketNotifier to monitor the read end of the socket
  socket_notifier_ = new QSocketNotifier(signal_fd_[1], QSocketNotifier::Read, this);
  connect(socket_notifier_, &QSocketNotifier::activated, this, &UnixSignalWatcher::HandleSignalNotification);

}

UnixSignalWatcher::~UnixSignalWatcher() {

  if (signal_fd_[0] != -1) {
    ::close(signal_fd_[0]);
  }
  if (signal_fd_[1] != -1) {
    ::close(signal_fd_[1]);
  }

}

void UnixSignalWatcher::WatchForSignal(const int signal) {

  if (watched_signals_.contains(signal)) {
    qLog(Debug) << "Already watching for signal" << signal;
    return;
  }

  struct sigaction signal_action{};
  signal_action.sa_handler = UnixSignalWatcher::SignalHandler;
  signal_action.sa_flags = SA_RESTART;
  if (::sigaction(signal, &signal_action, nullptr) != 0) {
    qLog(Error) << "sigaction error:" << ::strerror(errno);
    return;
  }

  watched_signals_ << signal;

}

void UnixSignalWatcher::SignalHandler(const int signal) {

  // Write the signal number to the socket pair (async-signal-safe)
  // This is the only operation we perform in the signal handler
  ::write(signal_fd_[0], &signal, sizeof(signal));

}

void UnixSignalWatcher::HandleSignalNotification() {

  // Read the signal number from the socket
  int signal = 0;
  const ssize_t bytes_read = ::read(signal_fd_[1], &signal, sizeof(signal));

  if (bytes_read == sizeof(signal)) {
    qLog(Debug) << "Caught signal:" << ::strsignal(signal);
    Q_EMIT UnixSignal(signal);
  }

}
