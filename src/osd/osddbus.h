/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OSDDBUS_H
#define OSDDBUS_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QImage>
#include <QDBusPendingCall>
#include <QDBusArgument>

#include "osdbase.h"

class OrgFreedesktopNotificationsInterface;
class QDBusPendingCallWatcher;

class Application;
class SystemTrayIcon;

class OSDDBus : public OSDBase {
  Q_OBJECT

 public:
  explicit OSDDBus(SystemTrayIcon *tray_icon, Application *app, QObject *parent = nullptr);
  ~OSDDBus() override;

  static const char *kSettingsGroup;

  bool SupportsNativeNotifications() override;
  bool SupportsTrayPopups() override;

 private:
  void Init();
  void ShowMessageNative(const QString &summary, const QString &message, const QString &icon = QString(), const QImage &image = QImage()) override;

 private slots:
  void CallFinished(QDBusPendingCallWatcher *watcher);

 private:
  std::unique_ptr<OrgFreedesktopNotificationsInterface> interface_;
  uint notification_id_;
  QDateTime last_notification_time_;

};

QDBusArgument &operator<<(QDBusArgument &arg, const QImage &image);
const QDBusArgument &operator>>(const QDBusArgument &arg, QImage &image);

#endif  // OSDDBUS_H
