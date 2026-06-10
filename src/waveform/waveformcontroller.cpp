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
#include "engine/enginebase.h"

#include "waveformcontroller.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"

using std::make_shared;

WaveformController::WaveformController(const SharedPtr<PlayerInterface> player, const SharedPtr<WaveformLoader> waveform_loader, QObject *parent)
    : QObject(parent),
      player_(player),
      waveform_loader_(waveform_loader) {}

void WaveformController::CurrentSongChanged(const Song &song) {

  // No enabled_ guard: the controller always loads, and the runtime show toggle
  // in WaveformProxyStyle gates rendering without blocking data generation.
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
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*pipeline, &WaveformPipeline::Finished, this, [this, connection, pipeline, song]() {
        AsyncLoadComplete(pipeline, song.url());
        QObject::disconnect(*connection);
      });
      break;
  }

}

void WaveformController::PlaybackStopped() {

  Q_EMIT CurrentWaveformDataChanged();

}

void WaveformController::AsyncLoadComplete(WaveformPipelinePtr pipeline, const QUrl &url) {

  // Is this song still playing?
  PlaylistItemPtr current_item = player_->GetCurrentItem();
  if (current_item && current_item->OriginalUrl() != url) {
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
