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

#include "config.h"

#include <chrono>

#include <QObject>
#include <QCoreApplication>
#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QDeadlineTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusError>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

#include "core/logging.h"
#include "sleepinhibitor.h"

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace {

constexpr char kPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalInhibitInterface[] = "org.freedesktop.portal.Inhibit";
constexpr char kPortalRequestInterface[] = "org.freedesktop.portal.Request";
constexpr uint kPortalInhibitSuspend = 4;
constexpr uint kPortalResponseSuccess = 0;

constexpr char kPowerManagementService[] = "org.freedesktop.PowerManagement";
constexpr char kPowerManagementPath[] = "/org/freedesktop/PowerManagement/Inhibit";
constexpr char kPowerManagementInterface[] = "org.freedesktop.PowerManagement.Inhibit";

constexpr char kLogin1Service[] = "org.freedesktop.login1";
constexpr char kLogin1Path[] = "/org/freedesktop/login1";
constexpr char kLogin1Interface[] = "org.freedesktop.login1.Manager";

// Keep the inhibitor for a while after playback stopped, so it is not released and requested again on every track change.
constexpr std::chrono::milliseconds kReleaseDelay = 10s;

// The portal reports the result of the inhibit request with the Response signal, a request the backend does not handle might never get a response.
constexpr std::chrono::milliseconds kPortalResponseTimeout = 5s;

}  // namespace

QString SleepInhibitor::BackendName(const Backend backend) {

  switch (backend) {
    case Backend::None:
      return u"none"_s;
    case Backend::Portal:
      return QLatin1String(kPortalInhibitInterface);
    case Backend::PowerManagement:
      return QLatin1String(kPowerManagementInterface);
    case Backend::Login1:
      return QLatin1String(kLogin1Interface);
  }

  return QString();

}

SleepInhibitor::SleepInhibitor(QObject *parent)
    : QObject(parent),
      timer_release_(new QTimer(this)),
      timer_portal_response_(new QTimer(this)),
      service_watcher_(new QDBusServiceWatcher(this)),
      enabled_(false),
      playing_(false),
      unavailable_(false),
      retry_acquire_(false),
      status_(Status::Released),
      backend_(Backend::None),
      acquiring_backend_(Backend::None),
      acquire_generation_(0),
      portal_request_counter_(0),
      powermanagement_cookie_(0) {

  timer_release_->setSingleShot(true);
  QObject::connect(timer_release_, &QTimer::timeout, this, &SleepInhibitor::ReleaseTimeout);

  timer_portal_response_->setSingleShot(true);
  timer_portal_response_->setInterval(kPortalResponseTimeout);
  QObject::connect(timer_portal_response_, &QTimer::timeout, this, &SleepInhibitor::PortalResponseTimeout);

  // The inhibitor is gone when the service holding it restarts, so watch the session services to request it again.
  // systemd-logind keeps its inhibitors when restarted, so it is not watched.
  service_watcher_->setConnection(QDBusConnection::sessionBus());
  service_watcher_->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
  service_watcher_->addWatchedService(QLatin1String(kPortalService));
  service_watcher_->addWatchedService(QLatin1String(kPowerManagementService));
  QObject::connect(service_watcher_, &QDBusServiceWatcher::serviceOwnerChanged, this, &SleepInhibitor::ServiceOwnerChanged);

}

SleepInhibitor::~SleepInhibitor() {

  if (status_ == Status::Held) {
    Release();
  }

}

void SleepInhibitor::SetEnabled(const bool enabled) {

  if (enabled == enabled_) return;

  enabled_ = enabled;
  // Try again when re-enabled, the service might be available now.
  unavailable_ = false;

  Update();

}

void SleepInhibitor::SetPlaying(const bool playing) {

  if (playing == playing_) return;

  playing_ = playing;

  // The release delay counts from when playback stopped, also when the inhibitor is acquired after that.
  if (!playing_) {
    release_deadline_.setRemainingTime(kReleaseDelay);
  }

  Update();

}

void SleepInhibitor::Update() {

  if (WantInhibit()) {
    timer_release_->stop();
    if (status_ == Status::Released && !unavailable_) {
      Acquire(Backend::Portal);
    }
    return;
  }

  // A request in flight is handled when it finishes.
  if (status_ != Status::Held) return;

  // Release right away when disabled, otherwise wait a while after playback stopped.
  if (!enabled_) {
    timer_release_->stop();
    Release();
  }
  else if (!timer_release_->isActive()) {
    const std::chrono::milliseconds remaining = std::chrono::ceil<std::chrono::milliseconds>(release_deadline_.remainingTimeAsDuration());
    if (remaining > 0ms) {
      timer_release_->start(remaining);
    }
    else {
      Release();
    }
  }

}

void SleepInhibitor::ReleaseTimeout() {

  if (!WantInhibit() && status_ == Status::Held) {
    Release();
  }

}

void SleepInhibitor::Acquire(const Backend backend) {

  const QString application = QCoreApplication::applicationName();
  const QString reason = tr("Playing music");

  QDBusConnection bus = QDBusConnection::sessionBus();
  QDBusMessage message;

  switch (backend) {
    case Backend::None:
      return;
    case Backend::Portal:{
      // The portal is the first backend tried, so a new request starts here.
      acquire_errors_.clear();
      // Subscribe to the Response signal before the request is made so the response is not missed, the request object path is predictable from the handle token.
      const QString handle_token = u"strawberry_sleep_inhibit_%1"_s.arg(++portal_request_counter_);
      QString sender = bus.baseService().mid(1);
      sender.replace(u'.', u'_');
      portal_handle_ = QDBusObjectPath(u"/org/freedesktop/portal/desktop/request/%1/%2"_s.arg(sender, handle_token));
      ConnectPortalResponse(portal_handle_.path());
      message = QDBusMessage::createMethodCall(QLatin1String(kPortalService), QLatin1String(kPortalPath), QLatin1String(kPortalInhibitInterface), u"Inhibit"_s);
      message << QString() << kPortalInhibitSuspend << QVariantMap{ { u"handle_token"_s, handle_token }, { u"reason"_s, reason } };
      break;
    }
    case Backend::PowerManagement:
      message = QDBusMessage::createMethodCall(QLatin1String(kPowerManagementService), QLatin1String(kPowerManagementPath), QLatin1String(kPowerManagementInterface), u"Inhibit"_s);
      message << application << reason;
      break;
    case Backend::Login1:
      bus = QDBusConnection::systemBus();
      message = QDBusMessage::createMethodCall(QLatin1String(kLogin1Service), QLatin1String(kLogin1Path), QLatin1String(kLogin1Interface), u"Inhibit"_s);
      message << u"sleep"_s << application << reason << u"block"_s;
      break;
  }

  status_ = Status::Acquiring;
  acquiring_backend_ = backend;
  // Replies and responses for an earlier request are ignored.
  const quint64 acquire_generation = ++acquire_generation_;

  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, backend, acquire_generation]() { AcquireFinished(watcher, backend, acquire_generation); });

}

void SleepInhibitor::AcquireFinished(QDBusPendingCallWatcher *watcher, const Backend backend, const quint64 acquire_generation) {

  watcher->deleteLater();

  if (acquire_generation != acquire_generation_ || status_ != Status::Acquiring) return;

  switch (backend) {
    case Backend::None:
      break;
    case Backend::Portal:{
      const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
      if (!reply.isValid()) {
        AcquireFailed(backend, reply.error().name() + u' ' + reply.error().message());
        return;
      }
      // Older portal versions do not use the handle token for the request object path.
      if (reply.value().path() != portal_handle_.path()) {
        DisconnectPortalResponse(portal_handle_.path());
        portal_handle_ = reply.value();
        ConnectPortalResponse(portal_handle_.path());
      }
      // Sleep is only inhibited once the portal responded with success.
      timer_portal_response_->start();
      return;
    }
    case Backend::PowerManagement:{
      const QDBusPendingReply<uint> reply = *watcher;
      if (!reply.isValid()) {
        AcquireFailed(backend, reply.error().name() + u' ' + reply.error().message());
        return;
      }
      powermanagement_cookie_ = reply.value();
      break;
    }
    case Backend::Login1:{
      const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;
      if (!reply.isValid() || !reply.value().isValid()) {
        AcquireFailed(backend, reply.error().name() + u' ' + reply.error().message());
        return;
      }
      login1_fd_ = reply.value();
      break;
    }
  }

  AcquireSucceeded(backend);

}

void SleepInhibitor::PortalResponse(const uint response, const QVariantMap &results) {

  Q_UNUSED(results)

  if (status_ != Status::Acquiring || acquiring_backend_ != Backend::Portal) return;

  timer_portal_response_->stop();
  DisconnectPortalResponse(portal_handle_.path());

  if (response == kPortalResponseSuccess) {
    AcquireSucceeded(Backend::Portal);
  }
  else {
    AcquireFailed(Backend::Portal, u"Response %1"_s.arg(response));
  }

}

void SleepInhibitor::PortalResponseTimeout() {

  if (status_ != Status::Acquiring || acquiring_backend_ != Backend::Portal) return;

  AcquireFailed(Backend::Portal, u"No response"_s);

}

void SleepInhibitor::AcquireSucceeded(const Backend backend) {

  qLog(Debug) << "Inhibited sleep using" << BackendName(backend);

  backend_ = backend;
  acquiring_backend_ = Backend::None;
  status_ = Status::Held;
  retry_acquire_ = false;

  // Playback might have stopped or the setting disabled while waiting for the reply.
  Update();

}

void SleepInhibitor::AcquireFailed(const Backend backend, const QString &error) {

  qLog(Debug) << "Failed to inhibit sleep using" << BackendName(backend) << error;
  acquire_errors_ << BackendName(backend) + u": "_s + error;

  acquiring_backend_ = Backend::None;
  status_ = Status::Released;

  if (backend == Backend::Portal) {
    timer_portal_response_->stop();
    ClosePortalRequest();
  }

  // Playback stopped or the setting was disabled while waiting, so do not try the next backend, the inhibitor is requested again when playback starts.
  if (!WantInhibit()) {
    retry_acquire_ = false;
    return;
  }

  switch (backend) {
    case Backend::Portal:
      Acquire(Backend::PowerManagement);
      return;
    case Backend::PowerManagement:
      Acquire(Backend::Login1);
      return;
    case Backend::Login1:
    case Backend::None:
      break;
  }

  // A service became available while the requests were in flight, so try once more.
  if (retry_acquire_) {
    retry_acquire_ = false;
    Acquire(Backend::Portal);
    return;
  }

  qLog(Warning) << "Unable to prevent the system from sleeping while playing, all sleep inhibitor D-Bus services failed:" << acquire_errors_.join(u"; "_s);
  unavailable_ = true;

}

void SleepInhibitor::ConnectPortalResponse(const QString &path) {

  QDBusConnection::sessionBus().connect(QLatin1String(kPortalService), path, QLatin1String(kPortalRequestInterface), u"Response"_s, this, SLOT(PortalResponse(uint, QVariantMap)));

}

void SleepInhibitor::DisconnectPortalResponse(const QString &path) {

  QDBusConnection::sessionBus().disconnect(QLatin1String(kPortalService), path, QLatin1String(kPortalRequestInterface), u"Response"_s, this, SLOT(PortalResponse(uint, QVariantMap)));

}

void SleepInhibitor::ClosePortalRequest() {

  if (portal_handle_.path().isEmpty()) return;

  DisconnectPortalResponse(portal_handle_.path());
  QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(QLatin1String(kPortalService), portal_handle_.path(), QLatin1String(kPortalRequestInterface), u"Close"_s));
  portal_handle_ = QDBusObjectPath();

}

void SleepInhibitor::ServiceOwnerChanged(const QString &service, const QString &old_owner, const QString &new_owner) {

  Q_UNUSED(old_owner)

  const bool held_by_service = status_ == Status::Held && ((backend_ == Backend::Portal && service == QLatin1String(kPortalService)) || (backend_ == Backend::PowerManagement && service == QLatin1String(kPowerManagementService)));

  if (held_by_service) {
    // The inhibitor went away with the old owner, so forget it without releasing it and request it again.
    qLog(Debug) << service << "restarted, requesting sleep inhibitor again";
    portal_handle_ = QDBusObjectPath();
    powermanagement_cookie_ = 0;
    backend_ = Backend::None;
    status_ = Status::Released;
    unavailable_ = false;
    Update();
    return;
  }

  if (new_owner.isEmpty()) return;

  // A service became available while requesting the inhibitor, so try again if the requests in flight fail.
  if (status_ == Status::Acquiring) {
    retry_acquire_ = true;
  }
  // A service became available, so try again if no service was available before.
  else if (unavailable_) {
    unavailable_ = false;
    Update();
  }

}

void SleepInhibitor::Release() {

  switch (backend_) {
    case Backend::None:
      break;
    case Backend::Portal:
      ClosePortalRequest();
      break;
    case Backend::PowerManagement:{
      QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(kPowerManagementService), QLatin1String(kPowerManagementPath), QLatin1String(kPowerManagementInterface), u"UnInhibit"_s);
      message << powermanagement_cookie_;
      QDBusConnection::sessionBus().send(message);
      powermanagement_cookie_ = 0;
      break;
    }
    case Backend::Login1:
      // The inhibitor is released when the last copy of the file descriptor is closed.
      login1_fd_ = QDBusUnixFileDescriptor();
      break;
  }

  qLog(Debug) << "Released sleep inhibitor";

  backend_ = Backend::None;
  status_ = Status::Released;

}
