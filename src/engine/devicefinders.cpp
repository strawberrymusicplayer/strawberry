/*
 * Strawberry Music Player
 * Copyright 2014, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtAlgorithms>
#include <QObject>
#include <QList>
#include <QtDebug>

#include "core/logging.h"
#include "devicefinders.h"
#include "devicefinder.h"

#ifdef HAVE_ALSA
#  include "alsadevicefinder.h"
#endif

#ifdef HAVE_LIBPULSE
#  include "pulsedevicefinder.h"
#endif

#ifdef Q_OS_MACOS
#  include "macosdevicefinder.h"
#endif

#ifdef Q_OS_WIN32
#  include "directsounddevicefinder.h"
#  include "mmdevicefinder.h"
#endif

DeviceFinders::DeviceFinders(QObject *parent) : QObject(parent) {}

DeviceFinders::~DeviceFinders() {
  qDeleteAll(device_finders_);
}

void DeviceFinders::Init() {

  QList<DeviceFinder*> device_finders;

#ifdef HAVE_ALSA
  device_finders.append(new AlsaDeviceFinder);
#endif
#ifdef HAVE_LIBPULSE
  device_finders.append(new PulseDeviceFinder);
#endif
#ifdef Q_OS_MACOS
  device_finders.append(new MacOsDeviceFinder);
#endif
#ifdef Q_OS_WIN32
  device_finders.append(new DirectSoundDeviceFinder);
  device_finders.append(new MMDeviceFinder);
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
