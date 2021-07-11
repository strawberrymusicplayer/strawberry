/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QVariant>
#include <QString>
#include <QtDebug>

#include <core/logging.h>

#include "devicefinder.h"
#include "alsapcmdevicefinder.h"

AlsaPCMDeviceFinder::AlsaPCMDeviceFinder() : DeviceFinder("alsa", {"alsa","alsasink"}) {}

QList<DeviceFinder::Device> AlsaPCMDeviceFinder::ListDevices() {

  QList<Device> ret;

  void **hints = nullptr;
  if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
    return ret;
  }

  for (void **n = hints ; *n ; ++n) {
    char *io = snd_device_name_get_hint(*n, "IOID");
    char *name = snd_device_name_get_hint(*n, "NAME");
    char *desc = snd_device_name_get_hint(*n, "DESC");
    if (io && name && desc && strcmp(io, "Output") == 0) {

      char *desc_last = desc;
      QString description;
      for (char *desc_i = desc ; desc_i && *desc_i != '\0' ; ++desc_i) {
        if (*desc_i == '\n') {
          *desc_i = '\0';
          if (!description.isEmpty()) description.append(' ');
          description.append(desc_last);
          desc_last = desc_i + 1;
        }
      }

      if (desc_last) {
        if (!description.isEmpty()) description.append(' ');
        description.append(desc_last);
      }

      Device device;
      device.value = name;
      device.description = description;
      device.iconname = GuessIconName(device.description);
      ret << device;  // clazy:exclude=reserve-candidates
    }
    if (io) free(io);
    if (name) free(name);
    if (desc) free(desc);
  }

  snd_device_name_free_hint(hints);

  return ret;

}
