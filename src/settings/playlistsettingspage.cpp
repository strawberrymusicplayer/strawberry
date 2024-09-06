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
#include "playlist/playlist.h"
#include "settingspage.h"
#include "playlistsettingspage.h"
#include "ui_playlistsettingspage.h"

class SettingsDialog;

const char *PlaylistSettingsPage::kSettingsGroup = "Playlist";

PlaylistSettingsPage::PlaylistSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_PlaylistSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("document-new"), true, 0, 32));

}

PlaylistSettingsPage::~PlaylistSettingsPage() {
  delete ui_;
}

void PlaylistSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  ui_->checkbox_alternating_row_colors->setChecked(s.value("alternating_row_colors", true).toBool());
  ui_->checkbox_barscurrenttrack->setChecked(s.value("show_bars", true).toBool());

  ui_->checkbox_glowcurrenttrack->setEnabled(ui_->checkbox_barscurrenttrack->isChecked());
  if (ui_->checkbox_barscurrenttrack->isChecked()) {
#ifdef Q_OS_MACOS
    bool glow_effect = false;
#else
    bool glow_effect = true;
#endif
    ui_->checkbox_glowcurrenttrack->setChecked(s.value("glow_effect", glow_effect).toBool());
  }

  ui_->checkbox_warncloseplaylist->setChecked(s.value("warn_close_playlist", true).toBool());
  ui_->checkbox_continueonerror->setChecked(s.value("continue_on_error", false).toBool());
  ui_->checkbox_greyout_songs_startup->setChecked(s.value("greyout_songs_startup", true).toBool());
  ui_->checkbox_greyout_songs_play->setChecked(s.value("greyout_songs_play", true).toBool());
  ui_->checkbox_select_track->setChecked(s.value("select_track", false).toBool());
  ui_->checkbox_show_toolbar->setChecked(s.value("show_toolbar", true).toBool());
  ui_->checkbox_playlist_clear->setChecked(s.value("playlist_clear", true).toBool());
  ui_->checkbox_auto_sort->setChecked(s.value("auto_sort", false).toBool());

  const PathType path_type = static_cast<PathType>(s.value("path_type", static_cast<int>(PathType::Automatic)).toInt());
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

  ui_->checkbox_editmetadatainline->setChecked(s.value("editmetadatainline", false).toBool());
  ui_->checkbox_writemetadata->setChecked(s.value("write_metadata", false).toBool());

  ui_->checkbox_delete_files->setChecked(s.value("delete_files", false).toBool());

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
  s.setValue("alternating_row_colors", ui_->checkbox_alternating_row_colors->isChecked());
  s.setValue("show_bars", ui_->checkbox_barscurrenttrack->isChecked());
  s.setValue("glow_effect", ui_->checkbox_glowcurrenttrack->isChecked());
  s.setValue("warn_close_playlist", ui_->checkbox_warncloseplaylist->isChecked());
  s.setValue("continue_on_error", ui_->checkbox_continueonerror->isChecked());
  s.setValue("greyout_songs_startup", ui_->checkbox_greyout_songs_startup->isChecked());
  s.setValue("greyout_songs_play", ui_->checkbox_greyout_songs_play->isChecked());
  s.setValue("select_track", ui_->checkbox_select_track->isChecked());
  s.setValue("show_toolbar", ui_->checkbox_show_toolbar->isChecked());
  s.setValue("playlist_clear", ui_->checkbox_playlist_clear->isChecked());
  s.setValue("path_type", static_cast<int>(path_type));
  s.setValue("editmetadatainline", ui_->checkbox_editmetadatainline->isChecked());
  s.setValue("write_metadata", ui_->checkbox_writemetadata->isChecked());
  s.setValue("delete_files", ui_->checkbox_delete_files->isChecked());
  s.setValue("auto_sort", ui_->checkbox_auto_sort->isChecked());
  s.endGroup();

}
