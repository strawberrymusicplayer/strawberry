// SPDX-License-Identifier: GPL-3.0-or-later

#include "browserauthentication_android.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QtCore/qcoreapplication_platform.h>

#include "core/logging.h"
#include "core/settings.h"

namespace {
constexpr char kServiceClass[] = "nl/dewitweb/strawberryflavor/BrowserAuthenticationService";
constexpr char kSettingsGroup[] = "AndroidBrowserAuthentication";
// Match BrowserAuthenticationService.TIMEOUT_MS, including time spent suspended.
constexpr qint64 kTimeoutMs = 120000;
}  // namespace

QPointer<AndroidBrowserAuthentication> AndroidBrowserAuthentication::instance_;

AndroidBrowserAuthentication::AndroidBrowserAuthentication(QObject *parent) : QObject(parent) {}

AndroidBrowserAuthentication::~AndroidBrowserAuthentication() {
  Stop();
}

bool AndroidBrowserAuthentication::Start(const QString &service, QString &error) {

  if (instance_) {
    error = tr("Another browser login is already in progress. Finish or cancel it first.");
    return false;
  }

  QJniEnvironment env;
  const JNINativeMethod methods[] = {
    {const_cast<char *>("nativeFinished"), const_cast<char *>("(JI)V"), reinterpret_cast<void *>(NativeFinished)},
  };
  if (!env.registerNativeMethods(kServiceClass, methods, 1)) {
    error = tr("Could not initialize Android browser authentication.");
    return false;
  }

  static qint64 next_request_id = 0;
  request_id_ = next_request_id = std::max(next_request_id + 1, QDateTime::currentMSecsSinceEpoch());
  service_ = service;
  started_at_ = QDateTime::currentMSecsSinceEpoch();
  instance_ = this;

  Settings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(service_, true);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    error = tr("Could not save the browser login status.");
    Stop();
    return false;
  }

  const bool started = QJniObject::callStaticMethod<jboolean>(
      kServiceClass, "start", "(Landroid/content/Context;JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
      QNativeInterface::QAndroidApplication::context().object(), static_cast<jlong>(request_id_),
      QJniObject::fromString(tr("Authentication")).object<jstring>(),
      QJniObject::fromString(tr("Signing in to %1").arg(service_)).object<jstring>(),
      QJniObject::fromString(tr("Finish signing in within two minutes. Return to Strawberry to cancel or retry.")).object<jstring>());
  if (env.checkAndClearExceptions() || !started) {
    error = tr("Could not keep browser authentication running in the background.");
    Stop();
    return false;
  }
  return true;

}

bool AndroidBrowserAuthentication::timed_out() const {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  return now < started_at_ || now - started_at_ >= kTimeoutMs;
}

void AndroidBrowserAuthentication::Stop() {

  if (!active()) return;

  instance_.clear();
  QJniEnvironment env;
  QJniObject::callStaticMethod<void>(kServiceClass, "stop", "(Landroid/content/Context;)V", QNativeInterface::QAndroidApplication::context().object());
  if (env.checkAndClearExceptions()) qLog(Error) << "Could not stop Android browser authentication";

  Settings settings;
  settings.beginGroup(kSettingsGroup);
  settings.remove(service_);
  settings.sync();
  if (settings.status() != QSettings::NoError) qLog(Error) << "Could not clear the browser login status";

}

bool AndroidBrowserAuthentication::WasInterrupted(const QString &service) {

  if (instance_) return false;
  Settings settings;
  settings.beginGroup(kSettingsGroup);
  const bool interrupted = settings.value(service, false).toBool();
  settings.remove(service);
  settings.sync();
  if (settings.status() != QSettings::NoError) qLog(Error) << "Could not read or clear the browser login status";
  return interrupted;

}

void AndroidBrowserAuthentication::NativeFinished(JNIEnv *, jclass, jlong request_id, jint reason) {

  if (!QMetaObject::invokeMethod(QCoreApplication::instance(), [request_id, reason]() {
        if (!instance_ || instance_->request_id_ != request_id) return;
        AndroidBrowserAuthentication *authentication = instance_;
        authentication->Stop();
        const QString error = reason == 1 ? tr("Authentication cancelled.")
                            : reason == 2 ? tr("Browser authentication timed out. Please try again.")
                                          : tr("Android could not start browser authentication.");
        Q_EMIT authentication->Failed(error);
      }, Qt::QueuedConnection)) {
    qLog(Error) << "Could not deliver the Android browser authentication result";
  }

}
