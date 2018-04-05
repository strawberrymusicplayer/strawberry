/*
 * Strawberry Music Player
 * Copyright 2017, Jonas Kvinge <jonas@jkvinge.net>
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

#include <stdio.h>
#include <string.h>
#include <QFile>
#include <alsa/asoundlib.h>

#include "alsadevicefinder.h"

#include <core/logging.h>

AlsaDeviceFinder::AlsaDeviceFinder()
    : DeviceFinder("alsa") {

}

QList<DeviceFinder::Device> AlsaDeviceFinder::ListDevices() {

  QList<Device> ret;

  //register int err;
  int card = -1;
  int dev = -1;
  int result = -1;
  snd_ctl_card_info_t *cardinfo;
  snd_pcm_info_t *pcminfo;
  snd_ctl_t *handle;

  snd_ctl_card_info_alloca(&cardinfo);
  snd_pcm_info_alloca(&pcminfo);

  card = -1;

  snd_pcm_stream_name(SND_PCM_STREAM_PLAYBACK);

  while (true) {

    result = snd_card_next(&card);
    if (result < 0) {
      qLog(Error) << "Unable to get soundcard: " << snd_strerror(result);
      return ret;
    }
    if (card < 0) return ret;

    char name[32];
    sprintf(name, "hw:%d", card);
    result = snd_ctl_open(&handle, name, 0);
    if (result < 0) {
      qLog(Error) << "Unable to open soundcard " << card << ": " << snd_strerror(result);
      continue;
    }
    result = snd_ctl_card_info(handle, cardinfo);
    if (result < 0) {
      qLog(Error) << "Control hardware failure for card " << card << ": " << snd_strerror(result);
      snd_ctl_close(handle);
      continue;
    }

    dev = -1;
    while (true) {

      result = snd_ctl_pcm_next_device(handle, &dev);
      if (result < 0) {
	qLog(Error) << "Unable to get PCM for card " << card << ": " << snd_strerror(result);
	continue;
      }
      if (dev < 0) break;

      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);
      snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
      result = snd_ctl_pcm_info(handle, pcminfo);
      if (result < 0) {
        if (result != -ENOENT) qLog(Error) << "Unable to get control digital audio info for card " << card << ": " << snd_strerror(result);
        continue;
      }

      snd_pcm_info_get_name(pcminfo);

      Device device;
      device.description = QString("%1 %2").arg(snd_ctl_card_info_get_name(cardinfo)).arg(snd_pcm_info_get_name(pcminfo));
      device.value = QString("hw:%1,%2").arg(card).arg(dev);
      device.iconname = GuessIconName(device.description);
      ret.append(device);

    }
    snd_ctl_close(handle);
  }

  snd_pcm_info_free(pcminfo); pcminfo = NULL;
  snd_ctl_card_info_free(cardinfo); cardinfo = NULL;

  snd_config_update_free_global();

  return ret;
}
