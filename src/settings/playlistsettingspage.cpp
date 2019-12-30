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


#include <QVariant>
#include <QSettings>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "playlist/playlist.h"
#include "settingspage.h"
#include "playlistsettingspage.h"
#include "ui_playlistsettingspage.h"

class SettingsDialog;

const char *PlaylistSettingsPage::kSettingsGroup = "Playlist";

PlaylistSettingsPage::PlaylistSettingsPage(SettingsDialog* dialog) : SettingsPage(dialog), ui_(new Ui_PlaylistSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("document-new"));

}

PlaylistSettingsPage::~PlaylistSettingsPage() {
  delete ui_;
}

void PlaylistSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

#ifdef Q_OS_MACOS
  bool glow_effect = false;
#else
  bool glow_effect = true;
#endif

  ui_->checkbox_glowcurrenttrack->setChecked(s.value("glow_effect", glow_effect).toBool());
  ui_->checkbox_warncloseplaylist->setChecked(s.value("warn_close_playlist", true).toBool());
  ui_->checkbox_continueonerror->setChecked(s.value("continue_on_error", false).toBool());
  ui_->checkbox_greyout_songs_startup->setChecked(s.value("greyout_songs_startup", true).toBool());
  ui_->checkbox_greyout_songs_play->setChecked(s.value("greyout_songs_play", true).toBool());
  ui_->checkbox_select_track->setChecked(s.value("select_track", false).toBool());

  Playlist::Path path = Playlist::Path(s.value(Playlist::kPathType, Playlist::Path_Automatic).toInt());
  switch (path) {
    case Playlist::Path_Automatic:
      ui_->radiobutton_automaticpath->setChecked(true);
      break;
    case Playlist::Path_Absolute:
      ui_->radiobutton_absolutepath->setChecked(true);
      break;
    case Playlist::Path_Relative:
      ui_->radiobutton_relativepath->setChecked(true);
      break;
    case Playlist::Path_Ask_User:
      ui_->radiobutton_askpath->setChecked(true);
  }

  ui_->checkbox_editmetadatainline->setChecked(s.value("editmetadatainline", false).toBool());
  ui_->checkbox_writemetadata->setChecked(s.value(Playlist::kWriteMetadata, false).toBool());

  s.endGroup();

}

void PlaylistSettingsPage::Save() {

  QSettings s;

  Playlist::Path path = Playlist::Path_Automatic;
  if (ui_->radiobutton_automaticpath->isChecked()) {
    path = Playlist::Path_Automatic;
  }
  else if (ui_->radiobutton_absolutepath->isChecked()) {
    path = Playlist::Path_Absolute;
  }
  else if (ui_->radiobutton_relativepath->isChecked()) {
    path = Playlist::Path_Relative;
  }
  else if (ui_->radiobutton_askpath->isChecked()) {
    path = Playlist::Path_Ask_User;
  }

  s.beginGroup(kSettingsGroup);
  s.setValue("glow_effect", ui_->checkbox_glowcurrenttrack->isChecked());
  s.setValue("warn_close_playlist", ui_->checkbox_warncloseplaylist->isChecked());
  s.setValue("continue_on_error", ui_->checkbox_continueonerror->isChecked());
  s.setValue("greyout_songs_startup", ui_->checkbox_greyout_songs_startup->isChecked());
  s.setValue("greyout_songs_play", ui_->checkbox_greyout_songs_play->isChecked());
  s.setValue("select_track", ui_->checkbox_select_track->isChecked());
  s.setValue(Playlist::kPathType, static_cast<int>(path));
  s.setValue("editmetadatainline", ui_->checkbox_editmetadatainline->isChecked());
  s.setValue(Playlist::kWriteMetadata, ui_->checkbox_writemetadata->isChecked());
  s.endGroup();

}
