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

#include <QObject>
#include <QByteArray>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/playerinterface.h"
#include "core/settings.h"
#include "engine/enginebase.h"
#include "constants/seekbarsettings.h"

#include "waveformcontroller.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"

using std::make_shared;

WaveformController::WaveformController(const SharedPtr<PlayerInterface> player, const SharedPtr<WaveformLoader> waveform_loader, QObject *parent)
    : QObject(parent),
      player_(player),
      waveform_loader_(waveform_loader),
      enabled_(false) {

  ReloadSettings();

}

void WaveformController::ReloadSettings() {

  Settings s;
  s.beginGroup(SeekbarSettings::kSettingsGroup);
  const bool enabled = static_cast<SeekbarSettings::Mode>(s.value(QLatin1String(SeekbarSettings::kMode), static_cast<int>(SeekbarSettings::kDefaultMode)).toInt()) == SeekbarSettings::Mode::Waveform;
  s.endGroup();

  const bool was_enabled = enabled_;
  enabled_ = enabled;

  ApplyEnabledTransition(was_enabled);

}

void WaveformController::CurrentSongChanged(const Song &song) {

  // Track the playing song even while disabled so enabling mid-track can generate the waveform for it without waiting for the next song change.
  current_song_ = song;

  if (!enabled_) return;

  GenerateWaveform(song);

}

void WaveformController::SetEnabled(const bool enabled) {

  if (enabled == enabled_) return;

  const bool was_enabled = enabled_;
  enabled_ = enabled;

  ApplyEnabledTransition(was_enabled);

}

void WaveformController::ApplyEnabledTransition(const bool was_enabled) {

  if (enabled_ && !was_enabled) {
    // Enabling mid-track: generate for whatever song is currently playing.
    if (!current_song_.url().isEmpty()) {
      GenerateWaveform(current_song_);
    }
  }
  else if (!enabled_ && was_enabled) {
    // Disabling reverts the seekbar to a normal slider and stops generation.
    Q_EMIT CurrentWaveformDataChanged();
  }

}

void WaveformController::GenerateWaveform(const Song &song) {

  const WaveformLoader::LoadResult load_result = waveform_loader_->Load(song.url(), song.has_cue());
  switch (load_result.status) {
    case WaveformLoader::LoadStatus::CannotLoad:
      Q_EMIT CurrentWaveformDataChanged();
      break;

    case WaveformLoader::LoadStatus::Loaded:
      Q_EMIT CurrentWaveformDataChanged(load_result.data);
      break;

    case WaveformLoader::LoadStatus::WillLoadAsync:
      // Emit an empty array for now so the seekbar reverts to a normal slider.  Our slot will be called when the data is actually loaded.
      Q_EMIT CurrentWaveformDataChanged();

      WaveformPipelinePtr pipeline = load_result.pipeline;
      Q_ASSERT(pipeline);
      const QUrl url = song.url();
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*pipeline, &WaveformPipeline::Finished, this, [this, connection, pipeline, url]() {
        AsyncLoadComplete(pipeline, url);
        QObject::disconnect(*connection);
      });
      break;
  }

}

void WaveformController::PlaybackStopped() {

  current_song_ = Song();

  if (enabled_) {
    Q_EMIT CurrentWaveformDataChanged();
  }

}

void WaveformController::AsyncLoadComplete(WaveformPipelinePtr pipeline, const QUrl &url) {

  // If settings were changed while the pipeline was in flight, suppress the emission — the user disabled the waveform and the seekbar should stay plain.
  if (!enabled_) return;

  // Is this song still playing?
  const PlaylistItemPtr current_item = player_->GetCurrentItem();
  if (!current_item || current_item->OriginalUrl() != url) {
    return;
  }
  // Did we stop the song?
  switch (player_->GetState()) {
    case EngineBase::State::Error:
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
      return;

    default:
      break;
  }

  Q_EMIT CurrentWaveformDataChanged(pipeline->data());

}
