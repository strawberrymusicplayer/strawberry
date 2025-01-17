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

#ifdef HAVE_GPOD
#  include <gpod/itdb.h>
#endif

#include <QThread>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QVariantList>
#include <QUrl>

#include "devicelister.h"

#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

DeviceLister::DeviceLister(QObject *parent)
    : QObject(parent),
      thread_(nullptr),
      original_thread_(nullptr),
      next_mount_request_id_(0) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

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
  thread_->setObjectName(objectName());
  QObject::connect(thread_, &QThread::started, this, &DeviceLister::ThreadStarted);

  moveToThread(thread_);
  thread_->start();
  qLog(Debug) << this << "moved to thread" << thread_;

}

void DeviceLister::ThreadStarted() { Init(); }

int DeviceLister::MountDeviceAsync(const QString &id) {

  const int request_id = next_mount_request_id_++;
  QMetaObject::invokeMethod(this, "MountDevice", Qt::QueuedConnection, Q_ARG(QString, id), Q_ARG(int, request_id));
  return request_id;

}

void DeviceLister::UnmountDeviceAsync(const QString &id) {
  QMetaObject::invokeMethod(this, "UnmountDevice", Qt::QueuedConnection, Q_ARG(QString, id));
}

void DeviceLister::MountDevice(const QString &id, const int request_id) {
  Q_EMIT DeviceMounted(id, request_id, true);
}

void DeviceLister::ExitAsync() {
  QMetaObject::invokeMethod(this, &DeviceLister::Exit, Qt::QueuedConnection);
}

void DeviceLister::Exit() {

  ShutDown();
  if (thread_) {
    moveToThread(original_thread_);
  }
  Q_EMIT ExitFinished();

}

namespace {

#ifdef HAVE_GPOD

QString GetIpodColour(Itdb_IpodModel model) {

  switch (model) {
    case ITDB_IPOD_MODEL_MINI_GREEN:
    case ITDB_IPOD_MODEL_NANO_GREEN:
    case ITDB_IPOD_MODEL_SHUFFLE_GREEN:
      return u"green"_s;

    case ITDB_IPOD_MODEL_MINI_BLUE:
    case ITDB_IPOD_MODEL_NANO_BLUE:
    case ITDB_IPOD_MODEL_SHUFFLE_BLUE:
      return u"blue"_s;

    case ITDB_IPOD_MODEL_MINI_PINK:
    case ITDB_IPOD_MODEL_NANO_PINK:
    case ITDB_IPOD_MODEL_SHUFFLE_PINK:
      return u"pink"_s;

    case ITDB_IPOD_MODEL_MINI_GOLD:
      return u"gold"_s;

    case ITDB_IPOD_MODEL_NANO_WHITE:
    case ITDB_IPOD_MODEL_VIDEO_WHITE:
      return u"white"_s;

    case ITDB_IPOD_MODEL_NANO_SILVER:
    case ITDB_IPOD_MODEL_CLASSIC_SILVER:
      return u"silver"_s;

    case ITDB_IPOD_MODEL_NANO_RED:
    case ITDB_IPOD_MODEL_SHUFFLE_RED:
      return u"red"_s;

    case ITDB_IPOD_MODEL_NANO_YELLOW:
      return u"yellow"_s;

    case ITDB_IPOD_MODEL_NANO_PURPLE:
    case ITDB_IPOD_MODEL_SHUFFLE_PURPLE:
      return u"purple"_s;

    case ITDB_IPOD_MODEL_NANO_ORANGE:
    case ITDB_IPOD_MODEL_SHUFFLE_ORANGE:
      return u"orange"_s;

    case ITDB_IPOD_MODEL_NANO_BLACK:
    case ITDB_IPOD_MODEL_VIDEO_BLACK:
    case ITDB_IPOD_MODEL_CLASSIC_BLACK:
      return u"black"_s;

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
      return u"mini"_s;

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
      return u"nano"_s;

    case ITDB_IPOD_MODEL_SHUFFLE:
    case ITDB_IPOD_MODEL_SHUFFLE_SILVER:
    case ITDB_IPOD_MODEL_SHUFFLE_PINK:
    case ITDB_IPOD_MODEL_SHUFFLE_BLUE:
    case ITDB_IPOD_MODEL_SHUFFLE_GREEN:
    case ITDB_IPOD_MODEL_SHUFFLE_ORANGE:
    case ITDB_IPOD_MODEL_SHUFFLE_RED:
      return u"shuffle"_s;

    case ITDB_IPOD_MODEL_COLOR:
    case ITDB_IPOD_MODEL_REGULAR:
    case ITDB_IPOD_MODEL_CLASSIC_SILVER:
    case ITDB_IPOD_MODEL_CLASSIC_BLACK:
      return u"standard"_s;

    case ITDB_IPOD_MODEL_COLOR_U2:
    case ITDB_IPOD_MODEL_REGULAR_U2:
      return u"U2"_s;

    default:
      return QString();
  }

}

#endif
}  // namespace

QUrl DeviceLister::MakeUrlFromLocalPath(const QString &path) const {

  if (IsIpod(path)) {
    QUrl ret;
    ret.setScheme(u"ipod"_s);
    ret.setPath(QDir::fromNativeSeparators(path));
    return ret;
  }

  return QUrl::fromLocalFile(path);

}

bool DeviceLister::IsIpod(const QString &path) {
  return QFile::exists(path + "/iTunes_Control"_L1) ||
         QFile::exists(path + "/iPod_Control"_L1) ||
         QFile::exists(path + "/iTunes/iTunes_Control"_L1);
}

QVariantList DeviceLister::GuessIconForPath(const QString &path) {

  QVariantList ret;

#ifdef HAVE_GPOD
  if (IsIpod(path)) {
    Itdb_Device *device = itdb_device_new();
    itdb_device_set_mountpoint(device, path.toLocal8Bit().constData());
    const Itdb_IpodInfo *info = itdb_device_get_ipod_info(device);

    if (info->ipod_model == ITDB_IPOD_MODEL_INVALID) {
      ret << u"device-ipod"_s;
    }
    else {
      QString model = GetIpodModel(info->ipod_model);
      QString colour = GetIpodColour(info->ipod_model);

      if (!model.isEmpty()) {
        QString model_icon = QStringLiteral("multimedia-player-ipod-%1").arg(model);
        if (QFile(model_icon).exists()) ret << model_icon;
        if (!colour.isEmpty()) {
          QString colour_icon = QStringLiteral("multimedia-player-ipod-%1-%2").arg(model, colour);
          if (QFile(colour_icon).exists()) ret << colour_icon;
        }
      }

      if (ret.isEmpty()) {
        ret << u"device-ipod"_s;
      }

    }

    itdb_device_free(device);

  }
#else
  Q_UNUSED(path)
#endif

  return ret;

}

QVariantList DeviceLister::GuessIconForModel(const QString &vendor, const QString &model) {

  QVariantList ret;
  if (vendor.startsWith("Google"_L1) && model.contains("Nexus"_L1)) {
    ret << u"phone-google-nexus-one"_s;
  }
  return ret;

}
