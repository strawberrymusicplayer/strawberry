/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/settings.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "constants/seekbarsettings.h"

#include "moodbarcontroller.h"
#include "moodbarloader.h"
#include "moodbarpipeline.h"

using std::make_shared;

MoodbarController::MoodbarController(const SharedPtr<Player> player, const SharedPtr<MoodbarLoader> moodbar_loader, QObject *parent)
    : QObject(parent),
      player_(player),
      moodbar_loader_(moodbar_loader),
      enabled_(false) {

  ReloadSettings();

}

void MoodbarController::ReloadSettings() {

  Settings s;
  s.beginGroup(SeekbarSettings::kSettingsGroup);
  const bool enabled = static_cast<SeekbarSettings::Mode>(s.value(QLatin1String(SeekbarSettings::kMode), static_cast<int>(SeekbarSettings::kDefaultMode)).toInt()) == SeekbarSettings::Mode::Moodbar;
  s.endGroup();

  const bool was_enabled = enabled_;
  enabled_ = enabled;

  ApplyEnabledTransition(was_enabled);

}

void MoodbarController::SetEnabled(const bool enabled) {

  if (enabled == enabled_) return;

  const bool was_enabled = enabled_;
  enabled_ = enabled;

  ApplyEnabledTransition(was_enabled);

}

void MoodbarController::ApplyEnabledTransition(const bool was_enabled) {

  if (enabled_ && !was_enabled) {
    // Enabling mid-track: generate for whatever song is currently playing.
    if (!current_song_.url().isEmpty()) {
      GenerateMoodbar(current_song_);
    }
  }
  else if (!enabled_ && was_enabled) {
    // Disabling reverts the seekbar to a normal slider and stops generation.
    Q_EMIT CurrentMoodbarDataChanged();
  }

}

void MoodbarController::CurrentSongChanged(const Song &song) {

  // Track the playing song even while disabled so enabling mid-track can generate the moodbar for it without waiting for the next song change.
  current_song_ = song;

  if (!enabled_) return;

  GenerateMoodbar(song);

}

void MoodbarController::GenerateMoodbar(const Song &song) {

  const MoodbarLoader::LoadResult load_result = moodbar_loader_->Load(song.url(), song.has_cue());
  switch (load_result.status) {
    case MoodbarLoader::LoadStatus::CannotLoad:
      Q_EMIT CurrentMoodbarDataChanged();
      break;

    case MoodbarLoader::LoadStatus::Loaded:
      Q_EMIT CurrentMoodbarDataChanged(load_result.data);
      break;

    case MoodbarLoader::LoadStatus::WillLoadAsync:
      // Emit an empty array for now so the GUI reverts to a normal progressbar.  Our slot will be called when the data is actually loaded.
      Q_EMIT CurrentMoodbarDataChanged();

      MoodbarPipelinePtr pipeline = load_result.pipeline;
      Q_ASSERT(pipeline);
      const QUrl url = song.url();
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*pipeline, &MoodbarPipeline::Finished, this, [this, connection, pipeline, url]() {
        AsyncLoadComplete(pipeline, url);
        QObject::disconnect(*connection);
      });
      break;
  }

}

void MoodbarController::PlaybackStopped() {

  current_song_ = Song();

  if (enabled_) {
    Q_EMIT CurrentMoodbarDataChanged();
  }

}

void MoodbarController::AsyncLoadComplete(MoodbarPipelinePtr pipeline, const QUrl &url) {

  // If the seekbar mode changed while the pipeline was in flight, suppress the emission — the seekbar should stay plain.
  if (!enabled_) return;

  // Is this song still playing?
  PlaylistItemPtr current_item = player_->GetCurrentItem();
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

  Q_EMIT CurrentMoodbarDataChanged(pipeline->data());

}
