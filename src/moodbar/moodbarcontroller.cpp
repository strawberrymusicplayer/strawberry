/* This file was part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QObject>
#include <QByteArray>
#include <QUrl>

#include "core/application.h"
#include "core/player.h"
#include "core/song.h"
#include "core/settings.h"
#include "engine/enginebase.h"
#include "settings/moodbarsettingspage.h"
#include "playlist/playlistmanager.h"

#include "moodbarcontroller.h"
#include "moodbarloader.h"
#include "moodbarpipeline.h"

MoodbarController::MoodbarController(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      enabled_(false) {

  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, this, &MoodbarController::CurrentSongChanged);
  QObject::connect(&*app_->player(), &Player::Stopped, this, &MoodbarController::PlaybackStopped);

  ReloadSettings();

}

void MoodbarController::ReloadSettings() {

  Settings s;
  s.beginGroup(MoodbarSettingsPage::kSettingsGroup);
  enabled_ = s.value("enabled", false).toBool();
  s.endGroup();

}

void MoodbarController::CurrentSongChanged(const Song &song) {

  if (!enabled_) return;

  QByteArray data;
  MoodbarPipeline *pipeline = nullptr;
  const MoodbarLoader::Result result = app_->moodbar_loader()->Load(song.url(), song.has_cue(), &data, &pipeline);

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
  PlaylistItemPtr current_item = app_->player()->GetCurrentItem();
  if (current_item && current_item->Url() != url) {
    return;
  }
  // Did we stop the song?
  switch (app_->player()->GetState()) {
    case EngineBase::State::Error:
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
      return;

    default:
      break;
  }

  Q_EMIT CurrentMoodbarDataChanged(pipeline->data());

}
