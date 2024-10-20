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

#include <alsa/asoundlib.h>

#include <QString>

#include <core/logging.h>

#include "alsapcmdevicefinder.h"
#include "enginedevice.h"

using namespace Qt::Literals::StringLiterals;

AlsaPCMDeviceFinder::AlsaPCMDeviceFinder() : DeviceFinder(u"alsa"_s, { u"alsa"_s, u"alsasink"_s }) {}

EngineDeviceList AlsaPCMDeviceFinder::ListDevices() {

  EngineDeviceList ret;

  void **hints = nullptr;
  if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
    return ret;
  }

  for (void **n = hints; *n; ++n) {
    char *hint_io = snd_device_name_get_hint(*n, "IOID");
    char *hint_name = snd_device_name_get_hint(*n, "NAME");
    char *hint_desc = snd_device_name_get_hint(*n, "DESC");
    if (hint_io && hint_name && hint_desc && strcmp(hint_io, "Output") == 0) {

      const QString name = QString::fromUtf8(hint_name);

      char *desc_last = hint_desc;
      QString description;
      for (char *desc_i = hint_desc; desc_i && *desc_i != '\0'; ++desc_i) {
        if (*desc_i == '\n') {
          *desc_i = '\0';
          if (!description.isEmpty()) description.append(u' ');
          description.append(QString::fromUtf8(desc_last));
          desc_last = desc_i + 1;
        }
      }

      if (desc_last) {
        if (!description.isEmpty()) description.append(u' ');
        description.append(QString::fromUtf8(desc_last));
      }

      EngineDevice device;
      device.value = name;
      device.description = description;
      device.iconname = device.GuessIconName();
      ret << device;  // clazy:exclude=reserve-candidates
    }
    if (hint_io) free(hint_io);
    if (hint_name) free(hint_name);
    if (hint_desc) free(hint_desc);
  }

  snd_device_name_free_hint(hints);

  return ret;

}
