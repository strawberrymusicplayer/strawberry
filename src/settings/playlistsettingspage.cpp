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

  ui_->combobox_doubleclickaddmode->setItemData(0, MainWindow::AddBehaviour_Append);
  ui_->combobox_doubleclickaddmode->setItemData(1, MainWindow::AddBehaviour_Load);
  ui_->combobox_doubleclickaddmode->setItemData(2, MainWindow::AddBehaviour_OpenInNew);
  ui_->combobox_doubleclickaddmode->setItemData(3, MainWindow::AddBehaviour_Enqueue);

  ui_->combobox_doubleclickplaymode->setItemData(0, MainWindow::PlayBehaviour_Never);
  ui_->combobox_doubleclickplaymode->setItemData(1, MainWindow::PlayBehaviour_IfStopped);
  ui_->combobox_doubleclickplaymode->setItemData(2, MainWindow::PlayBehaviour_Always);

  ui_->combobox_menuplaymode->setItemData(0, MainWindow::PlayBehaviour_Never);
  ui_->combobox_menuplaymode->setItemData(1, MainWindow::PlayBehaviour_IfStopped);
  ui_->combobox_menuplaymode->setItemData(2, MainWindow::PlayBehaviour_Always);

}

PlaylistSettingsPage::~PlaylistSettingsPage() {
  delete ui_;
}

void PlaylistSettingsPage::Load() {

  QSettings s;

  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  ui_->combobox_doubleclickaddmode->setCurrentIndex(ui_->combobox_doubleclickaddmode->findData(s.value("doubleclick_addmode", MainWindow::AddBehaviour_Append).toInt()));
  ui_->combobox_doubleclickplaymode->setCurrentIndex(ui_->combobox_doubleclickplaymode->findData(s.value("doubleclick_playmode", MainWindow::PlayBehaviour_Never).toInt()));
  ui_->combobox_menuplaymode->setCurrentIndex(ui_->combobox_menuplaymode->findData(s.value("menu_playmode", MainWindow::PlayBehaviour_Never).toInt()));
  ui_->checkbox_greyoutdeleted->setChecked(s.value("greyoutdeleted", false).toBool());

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

  ui_->checkbox_warncloseplaylist->setChecked(s.value("warn_close_playlist", true).toBool());
  ui_->checkbox_editmetadatainline->setChecked(s.value("editmetadatainline", false).toBool());
  ui_->checkbox_writemetadata->setChecked(s.value(Playlist::kWriteMetadata, false).toBool());

  s.endGroup();

}

void PlaylistSettingsPage::Save() {

  QSettings s;

  MainWindow::AddBehaviour doubleclick_addmode = MainWindow::AddBehaviour(ui_->combobox_doubleclickaddmode->itemData(ui_->combobox_doubleclickaddmode->currentIndex()).toInt());
  MainWindow::PlayBehaviour doubleclick_playmode = MainWindow::PlayBehaviour(ui_->combobox_doubleclickplaymode->itemData(ui_->combobox_doubleclickplaymode->currentIndex()).toInt());
  MainWindow::PlayBehaviour menu_playmode = MainWindow::PlayBehaviour(ui_->combobox_menuplaymode->itemData(ui_->combobox_menuplaymode->currentIndex()).toInt());

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

  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  s.setValue("doubleclick_addmode", doubleclick_addmode);
  s.setValue("doubleclick_playmode", doubleclick_playmode);
  s.setValue("menu_playmode", menu_playmode);
  s.setValue("greyoutdeleted", ui_->checkbox_greyoutdeleted->isChecked());
  s.setValue(Playlist::kPathType, static_cast<int>(path));
  s.setValue("warn_close_playlist", ui_->checkbox_warncloseplaylist->isChecked());
  s.setValue("editmetadatainline", ui_->checkbox_editmetadatainline->isChecked());
  s.setValue(Playlist::kWriteMetadata, ui_->checkbox_writemetadata->isChecked());
  s.endGroup();

}
