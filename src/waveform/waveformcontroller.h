/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#ifndef WAVEFORMCONTROLLER_H
#define WAVEFORMCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "waveform/waveformpipeline.h"

class WaveformLoader;
class Song;
class Player;

// Orchestrates waveform data delivery for the currently playing track. Connects
// Player::CurrentSongChanged to WaveformLoader::Load and emits
// CurrentWaveformDataChanged so WaveformProxyStyle can repaint the seekbar.
class WaveformController : public QObject {
  Q_OBJECT

 public:
  explicit WaveformController(const SharedPtr<Player> player, const SharedPtr<WaveformLoader> waveform_loader, QObject *parent = nullptr);

 Q_SIGNALS:
  // An empty byte array means there's no waveform, so the seekbar reverts to a normal slider.
  void CurrentWaveformDataChanged(const QByteArray &data = QByteArray());

 public Q_SLOTS:
  void CurrentSongChanged(const Song &song);
  void PlaybackStopped();

 private:
  void AsyncLoadComplete(WaveformPipelinePtr pipeline, const QUrl &url);

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<WaveformLoader> waveform_loader_;
};

#endif  // WAVEFORMCONTROLLER_H
