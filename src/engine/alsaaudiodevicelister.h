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

#ifndef ALSAAUDIODEVICELISTER_H
#define ALSAAUDIODEVICELISTER_H

#include "config.h"

#include <QString>

#include "audiodevicelister.h"
#include "enginedevice.h"

class AlsaAudioDeviceLister : public AudioDeviceLister {
 public:
  explicit AlsaAudioDeviceLister();

  bool Initialize() override { return true; }
  EngineDeviceList ListDevices() override;

  // Returns the "hw" and "plughw" devices for a PCM device of a card.
  // The value is named by the card ID, which doesn't change when cards are added or removed, the value with the card number is an alternative value.
  static EngineDeviceList MakeDevices(const QString &description, const int card, const QString &card_id, const int device_number);

 private:
  Q_DISABLE_COPY(AlsaAudioDeviceLister)
};

#endif  // ALSAAUDIODEVICELISTER_H
