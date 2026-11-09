/*
 * Strawberry Music Player
 * Copyright 2017-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstdio>
#include <cerrno>
#include <alsa/asoundlib.h>

#include <QString>
#include <QStringList>
#include <QScopeGuard>

#include <core/logging.h>

#include "alsaaudiodevicelister.h"
#include "enginedevice.h"

using namespace Qt::Literals::StringLiterals;

AlsaAudioDeviceLister::AlsaAudioDeviceLister() : AudioDeviceLister(u"alsa"_s, { u"alsa"_s, u"alsasink"_s }) {}

EngineDeviceList AlsaAudioDeviceLister::ListDevices() {

  EngineDeviceList devices;

  snd_pcm_stream_name(SND_PCM_STREAM_PLAYBACK);

  int card = -1;
  snd_ctl_card_info_t *cardinfo = nullptr;
  snd_ctl_card_info_alloca(&cardinfo);
  while (true) {

    int result = snd_card_next(&card);
    if (result < 0) {
      qLog(Error) << "Unable to get soundcard:" << snd_strerror(result);
      break;
    }
    if (card < 0) break;

    char str[32];
    snprintf(str, sizeof(str), "hw:%d", card);

    snd_ctl_t *handle = nullptr;
    result = snd_ctl_open(&handle, str, 0);
    if (result < 0) {
      qLog(Error) << "Unable to open soundcard" << card << ":" << snd_strerror(result);
      continue;
    }
    const QScopeGuard snd_ctl_handle_close = qScopeGuard([&handle]() { snd_ctl_close(handle); });

    result = snd_ctl_card_info(handle, cardinfo);
    if (result < 0) {
      qLog(Error) << "Control hardware failure for card" << card << ":" << snd_strerror(result);
      continue;
    }

    int dev = -1;
    snd_pcm_info_t *pcminfo = nullptr;
    snd_pcm_info_alloca(&pcminfo);
    while (true) {

      result = snd_ctl_pcm_next_device(handle, &dev);
      if (result < 0) {
        qLog(Error) << "Unable to get PCM for card" << card << ":" << snd_strerror(result);
        break;
      }
      if (dev < 0) break;

      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);

      result = snd_ctl_pcm_info(handle, pcminfo);
      if (result < 0) {
        if (result != -ENOENT) qLog(Error) << "Unable to get control digital audio info for card" << card << ":" << snd_strerror(result);
        continue;
      }

      const QString description = QStringLiteral("%1 %2").arg(QString::fromUtf8(snd_ctl_card_info_get_name(cardinfo)), QString::fromUtf8(snd_pcm_info_get_name(pcminfo)));
      devices << MakeDevices(description, card, QString::fromUtf8(snd_ctl_card_info_get_id(cardinfo)), dev);

    }
  }

  snd_config_update_free_global();

  return devices;

}

EngineDeviceList AlsaAudioDeviceLister::MakeDevices(const QString &description, const int card, const QString &card_id, const int device_number) {

  EngineDevice device;
  device.description = description;
  device.iconname = device.GuessIconName();
  device.card = card;
  device.device = device_number;

  EngineDeviceList devices;
  for (const QString &plugin : {u"hw"_s, u"plughw"_s}) {
    device.value = QStringLiteral("%1:%2,%3").arg(plugin, card_id).arg(device_number);
    device.alternative_values = QStringList() << QStringLiteral("%1:%2,%3").arg(plugin).arg(card).arg(device_number);
    devices << device;
  }

  return devices;

}
