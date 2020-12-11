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

#include "config.h"

#include <memory>

#include <dbus/notification.h>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QImage>
#include <QCoreApplication>
#include <QVersionNumber>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QJsonObject>
#include <QtDebug>

#include "core/logging.h"
#include "osddbus.h"

QDBusArgument &operator<<(QDBusArgument &arg, const QImage &image) {

  if (image.isNull()) {
    // Sometimes this gets called with a null QImage for no obvious reason.
    arg.beginStructure();
    arg << 0 << 0 << 0 << false << 0 << 0 << QByteArray();
    arg.endStructure();
    return arg;
  }
  QImage scaled = image.scaledToHeight(100, Qt::SmoothTransformation);

  scaled = scaled.convertToFormat(QImage::Format_ARGB32);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
  // ABGR -> ARGB
  QImage i = scaled.rgbSwapped();
#else
  // ABGR -> GBAR
  QImage i(scaled.size(), scaled.format());
  for (int y = 0; y < i.height(); ++y) {
    QRgb *p = (QRgb*)scaled.scanLine(y);
    QRgb *q = (QRgb*)i.scanLine(y);
    QRgb *end = p + scaled.width();
    while (p < end) {
      *q = qRgba(qGreen(*p), qBlue(*p), qAlpha(*p), qRed(*p));
      p++;
      q++;
    }
  }
#endif

  arg.beginStructure();
  arg << static_cast<qint32>(i.width());
  arg << static_cast<qint32>(i.height());
  arg << static_cast<qint32>(i.bytesPerLine());
  arg << i.hasAlphaChannel();
  qint32 channels = i.isGrayscale() ? 1 : (i.hasAlphaChannel() ? 4 : 3);
  qint32 bitspersample = i.depth() / channels;
  arg << bitspersample;
  arg << channels;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
  arg << QByteArray(reinterpret_cast<const char*>(i.constBits()), static_cast<int>(i.sizeInBytes()));
#else
  arg << QByteArray(reinterpret_cast<const char*>(i.constBits()), i.byteCount());
#endif
  arg.endStructure();

  return arg;

}

const QDBusArgument &operator>>(const QDBusArgument &arg, QImage &image) {

  Q_UNUSED(image);

  // This is needed to link but shouldn't be called.
  Q_ASSERT(0);
  return arg;

}

OSDDBus::OSDDBus(SystemTrayIcon *tray_icon, Application *app, QObject *parent) : OSDBase(tray_icon, app, parent), version_(1, 1), notification_id_(0) {
  Init();
}

OSDDBus::~OSDDBus() = default;

void OSDDBus::Init() {

  interface_.reset(new OrgFreedesktopNotificationsInterface(OrgFreedesktopNotificationsInterface::staticInterfaceName(), "/org/freedesktop/Notifications", QDBusConnection::sessionBus()));
  if (!interface_->isValid()) {
    qLog(Warning) << "Error connecting to notifications service.";
  }

  QString vendor, version, spec_version;
  QDBusReply<QString> reply = interface_->GetServerInformation(vendor, version, spec_version);
  if (reply.isValid()) {
    version_ = QVersionNumber::fromString(spec_version);
  }
  else {
    qLog(Error) << "Could not retrieve notification server information." << reply.error();
  }

}

bool OSDDBus::SupportsNativeNotifications() { return true; }

bool OSDDBus::SupportsTrayPopups() { return true; }

void OSDDBus::ShowMessageNative(const QString &summary, const QString &message, const QString &icon, const QImage &image) {

  if (!interface_) return;

  QVariantMap hints;
  QString summary_stripped = summary;
  summary_stripped = summary_stripped.remove(QRegularExpression("[&\"<>]")).simplified();

  if (!image.isNull()) {
    if (version_ >= QVersionNumber(1, 2)) {
      hints["image-data"] = QVariant(image);
    }
    else if (version_ >= QVersionNumber(1, 1)) {
      hints["image_data"] = QVariant(image);
    }
    else {
      hints["icon_data"] = QVariant(image);
    }
  }

  hints["transient"] = QVariant(true);

  int id = 0;
  if (last_notification_time_.secsTo(QDateTime::currentDateTime()) * 1000 < timeout_msec()) {
    // Reuse the existing popup if it's still open.  The reason we don't always
    // reuse the popup is because the notification daemon on KDE4 won't re-show the bubble if it's already gone to the tray.  See issue #118
    id = notification_id_;
  }

  QDBusPendingReply<uint> reply = interface_->Notify(app_name(), id, icon, summary_stripped, message, QStringList(), hints, timeout_msec());
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(CallFinished(QDBusPendingCallWatcher*)));

}

void OSDDBus::CallFinished(QDBusPendingCallWatcher *watcher) {

  std::unique_ptr<QDBusPendingCallWatcher> w(watcher);

  QDBusPendingReply<uint> reply = *w.get();
  if (reply.isError()) {
    qLog(Warning) << "Error sending notification" << reply.error().name();
    return;
  }

  uint id = reply.value();
  if (id != 0) {
    notification_id_ = id;
    last_notification_time_ = QDateTime::currentDateTime();
  }

}

