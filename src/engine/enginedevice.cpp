/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QtAlgorithms>
#include <QObject>
#include <QList>
#include <QtDebug>

#include "core/logging.h"
#include "devicefinder.h"
#include "enginedevice.h"

#ifdef HAVE_ALSA
#  include "alsadevicefinder.h"
#endif

#ifdef HAVE_LIBPULSE
#  include "pulsedevicefinder.h"
#endif

#ifdef Q_OS_DARWIN
#  include "osxdevicefinder.h"
#endif

#ifdef Q_OS_WIN32
#  include "directsounddevicefinder.h"
#endif

EngineDevice::EngineDevice(QObject *parent) : QObject(parent) {
}

EngineDevice::~EngineDevice() {
  qDeleteAll(device_finders_);
}

void EngineDevice::Init() {

  QList<DeviceFinder*> device_finders;

#ifdef HAVE_ALSA
  device_finders.append(new AlsaDeviceFinder);
#endif
#ifdef HAVE_LIBPULSE
  device_finders.append(new PulseDeviceFinder);
#endif
#ifdef Q_OS_DARWIN
  device_finders.append(new OsxDeviceFinder);
#endif
#ifdef Q_OS_WIN32
  device_finders.append(new DirectSoundDeviceFinder);
#endif

  for (DeviceFinder *finder : device_finders) {
    if (!finder->Initialise()) {
      qLog(Warning) << "Failed to initialise DeviceFinder for" << finder->name();
      delete finder;
      continue;
    }

    device_finders_.append(finder);
  }

}
