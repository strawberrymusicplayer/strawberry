/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifdef HAVE_LIBGPOD
#  include <gpod/itdb.h>
#endif

#include <QThread>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
#include <QUrl>

#include "devicelister.h"

#include "core/logging.h"

DeviceLister::DeviceLister() :
  thread_(nullptr),
  original_thread_(nullptr),
  next_mount_request_id_(0) {

  original_thread_ = thread();

}

DeviceLister::~DeviceLister() {

  if (thread_) {
    thread_->quit();
    thread_->wait(1000);
    thread_->deleteLater();
  }

}

void DeviceLister::Start() {

  thread_ = new QThread;
  connect(thread_, SIGNAL(started()), SLOT(ThreadStarted()));

  moveToThread(thread_);
  thread_->start();
  qLog(Debug) << this << "moved to thread" << thread_;

}

void DeviceLister::ThreadStarted() { Init(); }

int DeviceLister::MountDeviceAsync(const QString &id) {

  const int request_id = next_mount_request_id_++;
  metaObject()->invokeMethod(this, "MountDevice", Qt::QueuedConnection, Q_ARG(QString, id), Q_ARG(int, request_id));
  return request_id;

}

void DeviceLister::UnmountDeviceAsync(const QString &id) {
  metaObject()->invokeMethod(this, "UnmountDevice", Qt::QueuedConnection, Q_ARG(QString, id));
}

void DeviceLister::MountDevice(const QString &id, const int request_id) {
  emit DeviceMounted(id, request_id, true);
}

void DeviceLister::ExitAsync() {
  metaObject()->invokeMethod(this, "Exit", Qt::QueuedConnection);
}

void DeviceLister::Exit() {

  ShutDown();
  if (thread_) {
    moveToThread(original_thread_);
  }
  emit ExitFinished();

}

namespace {

#ifdef HAVE_LIBGPOD

QString GetIpodColour(Itdb_IpodModel model) {

  switch (model) {
    case ITDB_IPOD_MODEL_MINI_GREEN:
    case ITDB_IPOD_MODEL_NANO_GREEN:
    case ITDB_IPOD_MODEL_SHUFFLE_GREEN:
      return "green";

    case ITDB_IPOD_MODEL_MINI_BLUE:
    case ITDB_IPOD_MODEL_NANO_BLUE:
    case ITDB_IPOD_MODEL_SHUFFLE_BLUE:
      return "blue";

    case ITDB_IPOD_MODEL_MINI_PINK:
    case ITDB_IPOD_MODEL_NANO_PINK:
    case ITDB_IPOD_MODEL_SHUFFLE_PINK:
      return "pink";

    case ITDB_IPOD_MODEL_MINI_GOLD:
      return "gold";

    case ITDB_IPOD_MODEL_NANO_WHITE:
    case ITDB_IPOD_MODEL_VIDEO_WHITE:
      return "white";

    case ITDB_IPOD_MODEL_NANO_SILVER:
    case ITDB_IPOD_MODEL_CLASSIC_SILVER:
      return "silver";

    case ITDB_IPOD_MODEL_NANO_RED:
    case ITDB_IPOD_MODEL_SHUFFLE_RED:
      return "red";

    case ITDB_IPOD_MODEL_NANO_YELLOW:
      return "yellow";

    case ITDB_IPOD_MODEL_NANO_PURPLE:
    case ITDB_IPOD_MODEL_SHUFFLE_PURPLE:
      return "purple";

    case ITDB_IPOD_MODEL_NANO_ORANGE:
    case ITDB_IPOD_MODEL_SHUFFLE_ORANGE:
      return "orange";

    case ITDB_IPOD_MODEL_NANO_BLACK:
    case ITDB_IPOD_MODEL_VIDEO_BLACK:
    case ITDB_IPOD_MODEL_CLASSIC_BLACK:
      return "black";

    default:
      return QString();
  }

}

QString GetIpodModel(Itdb_IpodModel model) {

  switch (model) {
    case ITDB_IPOD_MODEL_MINI:
    case ITDB_IPOD_MODEL_MINI_BLUE:
    case ITDB_IPOD_MODEL_MINI_PINK:
    case ITDB_IPOD_MODEL_MINI_GREEN:
    case ITDB_IPOD_MODEL_MINI_GOLD:
      return "mini";

    case ITDB_IPOD_MODEL_NANO_WHITE:
    case ITDB_IPOD_MODEL_NANO_BLACK:
    case ITDB_IPOD_MODEL_NANO_SILVER:
    case ITDB_IPOD_MODEL_NANO_BLUE:
    case ITDB_IPOD_MODEL_NANO_GREEN:
    case ITDB_IPOD_MODEL_NANO_PINK:
    case ITDB_IPOD_MODEL_NANO_RED:
    case ITDB_IPOD_MODEL_NANO_YELLOW:
    case ITDB_IPOD_MODEL_NANO_PURPLE:
    case ITDB_IPOD_MODEL_NANO_ORANGE:
      return "nano";

    case ITDB_IPOD_MODEL_SHUFFLE:
    case ITDB_IPOD_MODEL_SHUFFLE_SILVER:
    case ITDB_IPOD_MODEL_SHUFFLE_PINK:
    case ITDB_IPOD_MODEL_SHUFFLE_BLUE:
    case ITDB_IPOD_MODEL_SHUFFLE_GREEN:
    case ITDB_IPOD_MODEL_SHUFFLE_ORANGE:
    case ITDB_IPOD_MODEL_SHUFFLE_RED:
      return "shuffle";

    case ITDB_IPOD_MODEL_COLOR:
    case ITDB_IPOD_MODEL_REGULAR:
    case ITDB_IPOD_MODEL_CLASSIC_SILVER:
    case ITDB_IPOD_MODEL_CLASSIC_BLACK:
      return "standard";

    case ITDB_IPOD_MODEL_COLOR_U2:
    case ITDB_IPOD_MODEL_REGULAR_U2:
      return "U2";

    default:
      return QString();
  }

}

#endif
}

QUrl DeviceLister::MakeUrlFromLocalPath(const QString &path) const {

  if (IsIpod(path)) {
    QUrl ret;
    ret.setScheme("ipod");
    ret.setPath(QDir::fromNativeSeparators(path));
    return ret;
  }

  return QUrl::fromLocalFile(path);

}

bool DeviceLister::IsIpod(const QString &path) const {
  return QFile::exists(path + "/iTunes_Control") ||
         QFile::exists(path + "/iPod_Control") ||
         QFile::exists(path + "/iTunes/iTunes_Control");
}

QStringList DeviceLister::GuessIconForPath(const QString &path) {

  QStringList ret;

#ifdef HAVE_LIBGPOD
  if (IsIpod(path)) {
    Itdb_Device* device = itdb_device_new();
    itdb_device_set_mountpoint(device, path.toLocal8Bit().constData());
    const Itdb_IpodInfo* info = itdb_device_get_ipod_info(device);

    if (info->ipod_model == ITDB_IPOD_MODEL_INVALID) {
      ret << "device-ipod";
    }
    else {
      QString model = GetIpodModel(info->ipod_model);
      QString colour = GetIpodColour(info->ipod_model);

      if (!model.isEmpty()) {
        QString model_icon = QString("multimedia-player-ipod-%1").arg(model);
        if (QFile(model_icon).exists()) ret << model_icon;
        if (!colour.isEmpty()) {
          QString colour_icon = QString("multimedia-player-ipod-%1-%2").arg(model, colour);
          if (QFile(colour_icon).exists()) ret << colour_icon;
        }
      }

      if (ret.isEmpty()) {
        ret << "device-ipod";
      }

    }

    itdb_device_free(device);

  }
#else
  Q_UNUSED(path)
#endif

  return ret;

}

QStringList DeviceLister::GuessIconForModel(const QString &vendor, const QString &model) {

  QStringList ret;
  if (vendor.startsWith("Google") && model.contains("Nexus")) {
    ret << "phone-google-nexus-one";
  }
  return ret;

}
