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

#ifndef AUDIODEVICELISTERS_H
#define AUDIODEVICELISTERS_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QString>

class AudioDeviceLister;

class AudioDeviceListers : public QObject {
  Q_OBJECT

 public:
  explicit AudioDeviceListers(QObject *parent = nullptr);
  ~AudioDeviceListers() override;

  void Init();
  QList<AudioDeviceLister*> ListListers() { return audio_device_listers_; }

 private:
  QList<AudioDeviceLister*> audio_device_listers_;
};

#endif  // AUDIODEVICELISTERS_H
