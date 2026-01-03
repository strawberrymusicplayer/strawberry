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
#include <atomic>

#include "core/logging.h"
#include "unixsignalwatcher.h"

namespace {
static std::atomic<UnixSignalWatcher*> sInstance{nullptr};
}  // namespace

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent) {

  UnixSignalWatcher *expected = nullptr;
  if (!sInstance.compare_exchange_strong(expected, this)) {
    Q_ASSERT(false && "UnixSignalWatcher singleton already exists");
  }

}

UnixSignalWatcher::~UnixSignalWatcher() {

  UnixSignalWatcher *expected = this;
  sInstance.compare_exchange_strong(expected, nullptr);

}

void UnixSignalWatcher::WatchForSignal(const int signal) {

  if (watched_signals_.contains(signal)) {
    qLog(Debug) << "Already watching for signal" << signal;
    return;
  }

  struct sigaction signal_action{};
  ::sigemptyset(&signal_action.sa_mask);
  signal_action.sa_handler = UnixSignalWatcher::SignalHandler;
  signal_action.sa_flags = SA_RESTART;
  if (::sigaction(signal, &signal_action, nullptr) != 0) {
    qLog(Debug) << "sigaction error: " << ::strerror(errno);
    return;
  }

  watched_signals_ << signal;

}

void UnixSignalWatcher::SignalHandler(const int signal) {

  qLog(Debug) << "Caught signal:" << ::strsignal(signal);

  UnixSignalWatcher *instance = sInstance.load(std::memory_order_acquire);
  if (instance) {
    Q_EMIT instance->UnixSignal(signal);
  }

}
