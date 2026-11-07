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

#ifndef SLEEPINHIBITOR_H
#define SLEEPINHIBITOR_H

#include "config.h"

#include <QObject>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QDeadlineTimer>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>

class QTimer;
class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

// Prevents the system from suspending while playing, so playback is not stopped by the automatic sleep on idle.
// The inhibitor is requested through the XDG desktop portal, falling back to org.freedesktop.PowerManagement and then systemd-logind.
// Only sleep is inhibited, the screen can still be turned off and locked.
class SleepInhibitor : public QObject {
  Q_OBJECT

 public:
  explicit SleepInhibitor(QObject *parent = nullptr);
  ~SleepInhibitor() override;

  void SetEnabled(const bool enabled);
  void SetPlaying(const bool playing);

 private:
  enum class Backend {
    None,
    Portal,
    PowerManagement,
    Login1
  };

  enum class Status {
    Released,
    Acquiring,
    Held
  };

  static QString BackendName(const Backend backend);
  bool WantInhibit() const { return enabled_ && playing_; }
  void Update();
  void ReleaseTimeout();
  void Acquire(const Backend backend);
  void AcquireFinished(QDBusPendingCallWatcher *watcher, const Backend backend, const quint64 acquire_generation);
  void AcquireSucceeded(const Backend backend);
  void AcquireFailed(const Backend backend, const QString &error);
  void PortalResponseTimeout();
  void ConnectPortalResponse(const QString &path);
  void DisconnectPortalResponse(const QString &path);
  void ClosePortalRequest();
  void ServiceOwnerChanged(const QString &service, const QString &old_owner, const QString &new_owner);
  void Release();

 private Q_SLOTS:
  void PortalResponse(const uint response, const QVariantMap &results);

 private:
  QTimer *timer_release_;
  QTimer *timer_portal_response_;
  QDBusServiceWatcher *service_watcher_;
  bool enabled_;
  bool playing_;
  bool unavailable_;
  bool retry_acquire_;
  QStringList acquire_errors_;
  QDeadlineTimer release_deadline_;
  Status status_;
  Backend backend_;
  Backend acquiring_backend_;
  quint64 acquire_generation_;
  quint64 portal_request_counter_;
  QDBusObjectPath portal_handle_;
  uint powermanagement_cookie_;
  QDBusUnixFileDescriptor login1_fd_;
};

#endif  // SLEEPINHIBITOR_H
