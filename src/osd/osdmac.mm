/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#import <UserNotifications/UserNotifications.h>

#include "config.h"

#include <memory>

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QString>

#include "includes/scoped_nsobject.h"

#include "core/logging.h"

#include "osdmac.h"

namespace {

bool NotificationCenterSupported() {
  return [UNUserNotificationCenter currentNotificationCenter] != nil;
}

void SendNotificationCenterMessage(NSString *title, NSString *subtitle) {

  scoped_nsobject<UNMutableNotificationContent> content([[UNMutableNotificationContent alloc] init]);
  [content setTitle:title];
  [content setSubtitle:subtitle];

  UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString] content:content.get() trigger:nil];

  [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request withCompletionHandler:^(NSError *error) {
    if (error) {
      qLog(Warning) << "Failed to deliver notification:" << QString::fromNSString(error.localizedDescription);
    }
  }];

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
