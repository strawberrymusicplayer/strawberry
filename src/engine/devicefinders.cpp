/*
 * Strawberry Music Player
 * Copyright 2014-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/logging.h"
#include "devicefinders.h"
#include "devicefinder.h"

#ifdef HAVE_ALSA
#  include "alsadevicefinder.h"
#  include "alsapcmdevicefinder.h"
#endif

#ifdef HAVE_PULSE
#  include "pulsedevicefinder.h"
#endif

#ifdef Q_OS_MACOS
#  include "macosdevicefinder.h"
#endif

#ifdef Q_OS_WIN32
#  include "directsounddevicefinder.h"
#  include "mmdevicefinder.h"
#  ifdef _MSC_VER
#    include "uwpdevicefinder.h"
#    include "asiodevicefinder.h"
#  endif  // _MSC_VER
#endif  // Q_OS_WIN32

using namespace Qt::Literals::StringLiterals;

DeviceFinders::DeviceFinders(QObject *parent) : QObject(parent) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

}

DeviceFinders::~DeviceFinders() {
  qDeleteAll(device_finders_);
}

void DeviceFinders::Init() {

  QList<DeviceFinder*> device_finders;

#ifdef HAVE_ALSA
  device_finders.append(new AlsaDeviceFinder);
  device_finders.append(new AlsaPCMDeviceFinder);
#endif
#ifdef HAVE_PULSE
  device_finders.append(new PulseDeviceFinder);
#endif
#ifdef Q_OS_MACOS
  device_finders.append(new MacOsDeviceFinder);
#endif
#ifdef Q_OS_WIN32
  device_finders.append(new DirectSoundDeviceFinder);
  device_finders.append(new MMDeviceFinder);
#  ifdef _MSC_VER
  device_finders.append(new UWPDeviceFinder);
  device_finders.append(new AsioDeviceFinder);
#  endif  // _MSC_VER
#endif  // Q_OS_WIN32

  for (DeviceFinder *finder : device_finders) {
    if (!finder->Initialize()) {
      qLog(Warning) << "Failed to initialize DeviceFinder for" << finder->name();
      delete finder;
      continue;
    }

    device_finders_.append(finder);
  }

}
