/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/song.h"
#include "core/settings.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "constants/moodbarsettings.h"

#include "moodbarcontroller.h"
#include "moodbarloader.h"
#include "moodbarpipeline.h"

MoodbarController::MoodbarController(const SharedPtr<Player> player, const SharedPtr<MoodbarLoader> moodbar_loader, QObject *parent)
    : QObject(parent),
      player_(player),
      moodbar_loader_(moodbar_loader),
      enabled_(false) {

  ReloadSettings();

}

void MoodbarController::ReloadSettings() {

  Settings s;
  s.beginGroup(MoodbarSettings::kSettingsGroup);
  enabled_ = s.value(MoodbarSettings::kEnabled, false).toBool();
  s.endGroup();

}

void MoodbarController::CurrentSongChanged(const Song &song) {

  if (!enabled_) return;

  QByteArray data;
  MoodbarPipeline *pipeline = nullptr;
  const MoodbarLoader::Result result = moodbar_loader_->Load(song.url(), song.has_cue(), &data, &pipeline);

  switch (result) {
    case MoodbarLoader::Result::CannotLoad:
      Q_EMIT CurrentMoodbarDataChanged(QByteArray());
      break;

    case MoodbarLoader::Result::Loaded:
      Q_EMIT CurrentMoodbarDataChanged(data);
      break;

    case MoodbarLoader::Result::WillLoadAsync:
      // Emit an empty array for now so the GUI reverts to a normal progress
      // bar.  Our slot will be called when the data is actually loaded.
      Q_EMIT CurrentMoodbarDataChanged(QByteArray());

      QObject::connect(pipeline, &MoodbarPipeline::Finished, this, [this, pipeline, song]() { AsyncLoadComplete(pipeline, song.url()); });
      break;
  }

}

void MoodbarController::PlaybackStopped() {

  if (enabled_) {
    Q_EMIT CurrentMoodbarDataChanged(QByteArray());
  }

}

void MoodbarController::AsyncLoadComplete(MoodbarPipeline *pipeline, const QUrl &url) {

  // Is this song still playing?
  PlaylistItemPtr current_item = player_->GetCurrentItem();
  if (current_item && current_item->Url() != url) {
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
