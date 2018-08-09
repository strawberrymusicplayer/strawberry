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

#include <stdbool.h>

#include <QWidget>
#include <QVariant>
#include <QString>
#include <QSettings>
#include <QCheckBox>
#include <QSpinBox>

#include "core/iconloader.h"
#include "core/logging.h"
#include "settingspage.h"
#include "playbacksettingspage.h"
#include "ui_playbacksettingspage.h"

class SettingsDialog;

const char *PlaybackSettingsPage::kSettingsGroup = "Playback";

PlaybackSettingsPage::PlaybackSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_PlaybackSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("media-play"));

  connect(ui_->checkbox_fadeout_stop, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->checkbox_fadeout_cross, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->checkbox_fadeout_auto, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));

}

PlaybackSettingsPage::~PlaybackSettingsPage() {
  delete ui_;
}

void PlaybackSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->checkbox_glowcurrenttrack->setChecked(s.value("glow_effect", true).toBool());
  ui_->checkbox_fadeout_stop->setChecked(s.value("FadeoutEnabled", false).toBool());
  ui_->checkbox_fadeout_cross->setChecked(s.value("CrossfadeEnabled", false).toBool());
  ui_->checkbox_fadeout_auto->setChecked(s.value("AutoCrossfadeEnabled", false).toBool());
  ui_->checkbox_fadeout_samealbum->setChecked(s.value("NoCrossfadeSameAlbum", true).toBool());
  ui_->checkbox_fadeout_pauseresume->setChecked(s.value("FadeoutPauseEnabled", false).toBool());
  ui_->spinbox_fadeduration->setValue(s.value("FadeoutDuration", 2000).toInt());
  ui_->spinbox_fadeduration_pauseresume->setValue(s.value("FadeoutPauseDuration", 250).toInt());
  s.endGroup();

}

void PlaybackSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue("glow_effect", ui_->checkbox_glowcurrenttrack->isChecked());
  s.setValue("FadeoutEnabled", ui_->checkbox_fadeout_stop->isChecked());
  s.setValue("CrossfadeEnabled", ui_->checkbox_fadeout_cross->isChecked());
  s.setValue("AutoCrossfadeEnabled", ui_->checkbox_fadeout_auto->isChecked());
  s.setValue("NoCrossfadeSameAlbum", ui_->checkbox_fadeout_samealbum->isChecked());
  s.setValue("FadeoutPauseEnabled", ui_->checkbox_fadeout_pauseresume->isChecked());
  s.setValue("FadeoutDuration", ui_->spinbox_fadeduration->value());
  s.setValue("FadeoutPauseDuration", ui_->spinbox_fadeduration_pauseresume->value());
  s.endGroup();
}

void PlaybackSettingsPage::FadingOptionsChanged() {

  ui_->widget_fading_options->setEnabled(ui_->checkbox_fadeout_stop->isChecked() || ui_->checkbox_fadeout_cross->isChecked() || ui_->checkbox_fadeout_auto->isChecked());

}
