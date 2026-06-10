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
#include "core/song.h"
#include "waveform/waveformpipeline.h"

class WaveformLoader;
class PlayerInterface;

// Orchestrates waveform data delivery for the currently playing track.
// Connects Player::CurrentSongChanged to WaveformLoader::Load and emits CurrentWaveformDataChanged so WaveformProxyStyle can repaint the seekbar.
//
// Generation is gated on the enabled state (mirroring MoodbarController): a track is only decoded when the waveform can actually be shown, so disabling the waveform avoids the background per-track decode cost.
// Enabling it mid-track re-generates for the song that is currently playing.
class WaveformController : public QObject {
  Q_OBJECT

 public:
  explicit WaveformController(const SharedPtr<PlayerInterface> player, const SharedPtr<WaveformLoader> waveform_loader, QObject *parent = nullptr);

  void ReloadSettings();

 Q_SIGNALS:
  // An empty byte array means there's no waveform, so the seekbar reverts to a normal slider.
  void CurrentWaveformDataChanged(const QByteArray &data = QByteArray());

 public Q_SLOTS:
  void CurrentSongChanged(const Song &song);
  void PlaybackStopped();

  // Driven by the seekbar toggle (WaveformProxyStyle::WaveformShow).
  // Enabling while a local song is playing triggers a load for that song; disabling clears the seekbar and stops further generation.
  void SetEnabled(const bool enabled);

 private:
  // Reconciles an enabled-state transition with the currently playing song.
  // Called by both SetEnabled() and ReloadSettings() so the "generate vs clear vs no-op" decision lives in exactly one place (WR-06).
  void ApplyEnabledTransition(const bool was_enabled);
  void GenerateWaveform(const Song &song);
  void AsyncLoadComplete(WaveformPipelinePtr pipeline, const QUrl &url);

 private:
  const SharedPtr<PlayerInterface> player_;
  const SharedPtr<WaveformLoader> waveform_loader_;
  bool enabled_;
  Song current_song_;
};

#endif  // WAVEFORMCONTROLLER_H
