/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QBuffer>
#include <QByteArray>
#include <QFile>

#include "includes/scoped_nsobject.h"

#include "osdmac.h"

namespace {

bool NotificationCenterSupported() {
  return NSClassFromString(@"NSUserNotificationCenter");
}

void SendNotificationCenterMessage(NSString *title, NSString *subtitle) {

  Class clazz = NSClassFromString(@"NSUserNotificationCenter");
  id notification_center = [clazz defaultUserNotificationCenter];

  Class user_notification_class = NSClassFromString(@"NSUserNotification");
  id notification = [[user_notification_class alloc] init];
  [notification setTitle:title];
  [notification setSubtitle:subtitle];

  [notification_center deliverNotification:notification];

}

}  // namespace

OSDMac::OSDMac(const SharedPtr<SystemTrayIcon> tray_icon, QObject *parent) : OSDBase(tray_icon, parent) {}

OSDMac::~OSDMac() = default;

bool OSDMac::SupportsNativeNotifications() const {
  return NotificationCenterSupported();
}

bool OSDMac::SupportsTrayPopups() const { return false; }

void OSDMac::ShowMessageNative(const QString &summary, const QString &message, const QString &icon, const QImage &image) {

  Q_UNUSED(icon);
  Q_UNUSED(image);

  if (NotificationCenterSupported()) {
    scoped_nsobject<NSString> mac_message([[NSString alloc] initWithUTF8String:message.toUtf8().constData()]);
    scoped_nsobject<NSString> mac_summary([[NSString alloc] initWithUTF8String:summary.toUtf8().constData()]);
    SendNotificationCenterMessage(mac_summary.get(), mac_message.get());
  }

}
