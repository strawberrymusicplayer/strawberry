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

#ifndef UNIXSIGNALWATCHER_H
#define UNIXSIGNALWATCHER_H

#include <csignal>

#include <QObject>
#include <QList>

class QSocketNotifier;

/**
 * @brief Watches for Unix process signals and forwards them as Qt signals.
 *
 * UnixSignalWatcher installs POSIX signal handlers and uses the "self-pipe"
 * trick to translate asynchronous Unix signals into events on a file
 * descriptor. A QSocketNotifier monitors that descriptor and invokes
 * HandleSignalNotification() in the Qt event loop thread, where the
 * UnixSignal() Qt signal is emitted.
 *
 * Typical usage:
 * - Create a single instance in the thread that owns the main Qt event loop.
 * - Call WatchForSignal() for each Unix signal you want to observe.
 * - Connect to the UnixSignal(int) signal to handle incoming Unix signals
 *   in a safe, event-driven manner.
 *
 * Thread-safety:
 * - UnixSignalWatcher is intended to be used from a single thread with an
 *   active Qt event loop (usually the main thread).
 * - The low-level POSIX signal handler writes to the self-pipe only, which
 *   is async-signal-safe; all higher-level processing happens in the event
 *   loop thread via QSocketNotifier.
 */
class UnixSignalWatcher : public QObject {
  Q_OBJECT

 public:
  /**
   * @brief Constructs a UnixSignalWatcher.
   *
   * The instance sets up the internal self-pipe and QSocketNotifier needed
   * to deliver Unix signals into the Qt event loop. No signals are watched
   * until WatchForSignal() is called.
   *
   * @param parent QObject parent for normal Qt ownership semantics.
   */
  explicit UnixSignalWatcher(QObject *parent = nullptr);
  ~UnixSignalWatcher() override;

  /**
   * @brief Registers a Unix signal to be watched.
   *
   * Installs or updates the POSIX signal handler for @p signal so that
   * delivery of that Unix signal is routed through the self-pipe and
   * subsequently reported via the UnixSignal(int) Qt signal.
   *
   * It is safe to call this multiple times for different signal numbers.
   *
   * @param signal The Unix signal number (e.g. SIGINT, SIGTERM).
   */
  void WatchForSignal(const int signal);

 Q_SIGNALS:
  /**
   * @brief Emitted when a watched Unix signal is received.
   *
   * This signal is emitted in the Qt event loop thread after the low-level
   * POSIX signal handler has written to the self-pipe and the corresponding
   * notification has been processed.
   *
   * @param signal The Unix signal number that was delivered.
   */
  void UnixSignal(const int signal);

 private:
  static void SignalHandler(const int signal);
  void HandleSignalNotification();

  static UnixSignalWatcher *sInstance;
  int signal_fd_[2];
  QSocketNotifier *socket_notifier_;
  QList<int> watched_signals_;
  QList<struct sigaction> original_signal_actions_;
};

#endif  // UNIXSIGNALWATCHER_H
