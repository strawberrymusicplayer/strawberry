// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectionwatcher.h"

#include <QByteArray>
#include <QGuiApplication>
#include <android/log.h>

void CollectionWatcher::FollowApplicationState() {

  if (!qGuiApp) return;
  // Nothing is monitored while the app is not in the foreground, the started monitors reconcile the changes made meanwhile.
  // Delivered directly on the main thread, so the monitors are cancelled while a scan keeps the watcher's thread busy.
  QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationSuspended && state != Qt::ApplicationActive) return;
    const bool suspended = state != Qt::ApplicationActive;
    if (suspended) {
      const QByteArray message = QByteArray("Stopping ") + QByteArray::number(CancelNasMonitors()) + " NAS monitors in the background";
      __android_log_write(ANDROID_LOG_INFO, "StrawberryNas", message.constData());
    }
    // Joining and starting run on the watcher's thread in the order of the changes, after the previous change was applied.
    QMetaObject::invokeMethod(this, [this, suspended]() {
      if (!SetNasMonitorsSuspended(suspended) || suspended) return;
      const QByteArray message = QByteArray("Started ") + QByteArray::number(nas_monitors_.size()) + " NAS monitors in the foreground";
      __android_log_write(ANDROID_LOG_INFO, "StrawberryNas", message.constData());
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

}
