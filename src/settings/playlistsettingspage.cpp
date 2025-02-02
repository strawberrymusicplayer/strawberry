/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QVariant>
#include <QSettings>
#include <QCheckBox>
#include <QRadioButton>

#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/playlistsettings.h"
#include "settingspage.h"
#include "playlistsettingspage.h"
#include "ui_playlistsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace PlaylistSettings;

class SettingsDialog;

PlaylistSettingsPage::PlaylistSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_PlaylistSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"document-new"_s, true, 0, 32));

}

PlaylistSettingsPage::~PlaylistSettingsPage() {
  delete ui_;
}

void PlaylistSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  ui_->checkbox_alternating_row_colors->setChecked(s.value(kAlternatingRowColors, true).toBool());
  ui_->checkbox_barscurrenttrack->setChecked(s.value(kShowBars, true).toBool());

  ui_->checkbox_glowcurrenttrack->setEnabled(ui_->checkbox_barscurrenttrack->isChecked());
  if (ui_->checkbox_barscurrenttrack->isChecked()) {
#ifdef Q_OS_MACOS
    bool glow_effect = false;
#else
    bool glow_effect = true;
#endif
    ui_->checkbox_glowcurrenttrack->setChecked(s.value(kGlowEffect, glow_effect).toBool());
  }

  ui_->checkbox_warncloseplaylist->setChecked(s.value(kWarnClosePlaylist, true).toBool());
  ui_->checkbox_continueonerror->setChecked(s.value(kContinueOnError, false).toBool());
  ui_->checkbox_greyout_songs_startup->setChecked(s.value(kGreyoutSongsStartup, false).toBool());
  ui_->checkbox_greyout_songs_play->setChecked(s.value(kGreyoutSongsPlay, true).toBool());
  ui_->checkbox_select_track->setChecked(s.value(kSelectTrack, false).toBool());
  ui_->checkbox_show_toolbar->setChecked(s.value(kShowToolbar, true).toBool());
  ui_->checkbox_playlist_clear->setChecked(s.value(kPlaylistClear, true).toBool());
  ui_->checkbox_auto_sort->setChecked(s.value(kAutoSort, false).toBool());

  const PathType path_type = static_cast<PathType>(s.value(kPathType, static_cast<int>(PathType::Automatic)).toInt());
  switch (path_type) {
    case PathType::Automatic:
      ui_->radiobutton_automaticpath->setChecked(true);
      break;
    case PathType::Absolute:
      ui_->radiobutton_absolutepath->setChecked(true);
      break;
    case PathType::Relative:
      ui_->radiobutton_relativepath->setChecked(true);
      break;
    case PathType::Ask_User:
      ui_->radiobutton_askpath->setChecked(true);
  }

  ui_->checkbox_editmetadatainline->setChecked(s.value(kEditMetadataInline, false).toBool());
  ui_->checkbox_writemetadata->setChecked(s.value(kWriteMetadata, false).toBool());

  ui_->checkbox_delete_files->setChecked(s.value(kDeleteFiles, false).toBool());

  s.endGroup();

  Init(ui_->layout_playlistsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void PlaylistSettingsPage::Save() {

  PathType path_type = PathType::Automatic;
  if (ui_->radiobutton_automaticpath->isChecked()) {
    path_type = PathType::Automatic;
  }
  else if (ui_->radiobutton_absolutepath->isChecked()) {
    path_type = PathType::Absolute;
  }
  else if (ui_->radiobutton_relativepath->isChecked()) {
    path_type = PathType::Relative;
  }
  else if (ui_->radiobutton_askpath->isChecked()) {
    path_type = PathType::Ask_User;
  }

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kAlternatingRowColors, ui_->checkbox_alternating_row_colors->isChecked());
  s.setValue(kShowBars, ui_->checkbox_barscurrenttrack->isChecked());
  s.setValue(kGlowEffect, ui_->checkbox_glowcurrenttrack->isChecked());
  s.setValue(kWarnClosePlaylist, ui_->checkbox_warncloseplaylist->isChecked());
  s.setValue(kContinueOnError, ui_->checkbox_continueonerror->isChecked());
  s.setValue(kGreyoutSongsStartup, ui_->checkbox_greyout_songs_startup->isChecked());
  s.setValue(kGreyoutSongsPlay, ui_->checkbox_greyout_songs_play->isChecked());
  s.setValue(kSelectTrack, ui_->checkbox_select_track->isChecked());
  s.setValue(kShowToolbar, ui_->checkbox_show_toolbar->isChecked());
  s.setValue(kPlaylistClear, ui_->checkbox_playlist_clear->isChecked());
  s.setValue(kPathType, static_cast<int>(path_type));
  s.setValue(kEditMetadataInline, ui_->checkbox_editmetadatainline->isChecked());
  s.setValue(kWriteMetadata, ui_->checkbox_writemetadata->isChecked());
  s.setValue(kDeleteFiles, ui_->checkbox_delete_files->isChecked());
  s.setValue(kAutoSort, ui_->checkbox_auto_sort->isChecked());
  s.endGroup();

}
