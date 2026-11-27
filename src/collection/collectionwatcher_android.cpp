// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectionwatcher.h"

#include <QByteArray>
#include <QGuiApplication>
#include <android/log.h>

void CollectionWatcher::FollowApplicationState() {

  if (!qGuiApp) return;
  // Nothing is monitored while the app is not in the foreground, the started monitors scan the folders changed meanwhile.
  // Delivered directly on the main thread, so the monitors are cancelled while a scan keeps the watcher's thread busy.
  QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationSuspended && state != Qt::ApplicationActive) return;
    const bool suspended = state != Qt::ApplicationActive;
    if (suspended) {
      const qsizetype cancelled = CancelNasMonitors();
      // A running NAS scan is stopped too, so the monitors start again promptly, scans already requested are skipped.
      const bool scan_stopped = nas_scan_running_;
      if (scan_stopped) Stop();
      const QByteArray message = QByteArray("Stopping ") + QByteArray::number(cancelled) + " NAS monitors" + (scan_stopped ? " and the running scan" : "") + " in the background";
      __android_log_write(ANDROID_LOG_INFO, "StrawberryNas", message.constData());
    }
    // Joining and starting run on the watcher's thread in the order of the changes, after the previous change was applied.
    QMetaObject::invokeMethod(this, [this, suspended]() {
      if (!SetNasMonitorsSuspended(suspended) || suspended) return;
      qsizetype started = 0;
      {
        const std::lock_guard<std::mutex> lock(nas_monitors_mutex_);
        started = nas_monitors_.size();
      }
      const QByteArray message = QByteArray("Started ") + QByteArray::number(started) + " NAS monitors in the foreground";
      __android_log_write(ANDROID_LOG_INFO, "StrawberryNas", message.constData());
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

}
