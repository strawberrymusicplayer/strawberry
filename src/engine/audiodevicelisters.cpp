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
#include "audiodevicelisters.h"
#include "audiodevicelister.h"

#ifdef HAVE_ALSA
#  include "alsaaudiodevicelister.h"
#  include "alsapcmaudiodevicelister.h"
#endif

#ifdef HAVE_PULSE
#  include "pulseaudiodevicelister.h"
#endif

#ifdef Q_OS_MACOS
#  include "macosaudiodevicelister.h"
#endif

#ifdef Q_OS_WIN32
#  include "directsoundaudiodevicelister.h"
#  include "mmaudiodevicelister.h"
#  include "asioaudiodevicelister.h"
#  ifdef _MSC_VER
#    include "uwpaudiodevicelister.h"
#  endif  // _MSC_VER
#endif  // Q_OS_WIN32

using namespace Qt::Literals::StringLiterals;

AudioDeviceListers::AudioDeviceListers(QObject *parent) : QObject(parent) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

}

AudioDeviceListers::~AudioDeviceListers() {
  qDeleteAll(audio_device_listers_);
}

void AudioDeviceListers::Init() {

  QList<AudioDeviceLister*> audio_device_listers;

#ifdef HAVE_ALSA
  audio_device_listers.append(new AlsaAudioDeviceLister);
  audio_device_listers.append(new AlsaPCMAudioDeviceLister);
#endif
#ifdef HAVE_PULSE
  audio_device_listers.append(new PulseAudioDeviceLister);
#endif
#ifdef Q_OS_MACOS
  audio_device_listers.append(new MacOsAudioDeviceLister);
#endif
#ifdef Q_OS_WIN32
  audio_device_listers.append(new DirectSoundAudioDeviceLister);
  audio_device_listers.append(new MMAudioDeviceLister);
  audio_device_listers.append(new AsioAudioDeviceLister);
#  ifdef _MSC_VER
  audio_device_listers.append(new UWPAudioDeviceLister);
#  endif  // _MSC_VER
#endif  // Q_OS_WIN32

  for (AudioDeviceLister *lister : audio_device_listers) {
    if (!lister->Initialize()) {
      qLog(Warning) << "Failed to initialize AudioDeviceLister for" << lister->name();
      delete lister;
      continue;
    }

    audio_device_listers_.append(lister);
  }

}
