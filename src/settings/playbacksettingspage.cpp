/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "playbacksettingspage.h"
#include "ui_playbacksettingspage.h"

#include "core/iconloader.h"
#include "settingsdialog.h"
#include "playlist/playlist.h"

const char *PlaybackSettingsPage::kSettingsGroup = "Playback";

PlaybackSettingsPage::PlaybackSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_PlaybackSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("media-play"));

  connect(ui_->fading_cross, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->fading_out, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->fading_auto, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));

}

PlaybackSettingsPage::~PlaybackSettingsPage() {

  delete ui_;

}

void PlaybackSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->current_glow->setChecked(s.value("glow_effect", true).toBool());
  ui_->fading_out->setChecked(s.value("FadeoutEnabled", false).toBool());
  ui_->fading_cross->setChecked(s.value("CrossfadeEnabled", false).toBool());
  ui_->fading_auto->setChecked(s.value("AutoCrossfadeEnabled", false).toBool());
  ui_->fading_duration->setValue(s.value("FadeoutDuration", 2000).toInt());
  ui_->fading_samealbum->setChecked(s.value("NoCrossfadeSameAlbum", true).toBool());
  ui_->fadeout_pause->setChecked(s.value("FadeoutPauseEnabled", false).toBool());
  ui_->fading_pause_duration->setValue(s.value("FadeoutPauseDuration", 250).toInt());
  s.endGroup();

}

void PlaybackSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue("glow_effect", ui_->current_glow->isChecked());
  s.setValue("FadeoutEnabled", ui_->fading_out->isChecked());
  s.setValue("FadeoutDuration", ui_->fading_duration->value());
  s.setValue("CrossfadeEnabled", ui_->fading_cross->isChecked());
  s.setValue("AutoCrossfadeEnabled", ui_->fading_auto->isChecked());
  s.setValue("NoCrossfadeSameAlbum", ui_->fading_samealbum->isChecked());
  s.setValue("FadeoutPauseEnabled", ui_->fadeout_pause->isChecked());
  s.setValue("FadeoutPauseDuration", ui_->fading_pause_duration->value());
  s.endGroup();
}

void PlaybackSettingsPage::FadingOptionsChanged() {

  ui_->fading_options->setEnabled(ui_->fading_out->isChecked() || ui_->fading_cross->isChecked() || ui_->fading_auto->isChecked());

}
