/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
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

#include <QWidget>
#include <QDialog>
#include <QVariant>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>

#include "playlist.h"
#include "playlistsaveoptionsdialog.h"
#include "ui_playlistsaveoptionsdialog.h"

const char *PlaylistSaveOptionsDialog::kSettingsGroup = "PlaylistSaveOptionsDialog";

PlaylistSaveOptionsDialog::PlaylistSaveOptionsDialog(QWidget *parent) : QDialog(parent), ui(new Ui::PlaylistSaveOptionsDialog) {

  ui->setupUi(this);

  ui->filePaths->addItem(tr("Automatic"), Playlist::Path_Automatic);
  ui->filePaths->addItem(tr("Relative"), Playlist::Path_Relative);
  ui->filePaths->addItem(tr("Absolute"), Playlist::Path_Absolute);
}

PlaylistSaveOptionsDialog::~PlaylistSaveOptionsDialog() { delete ui; }

void PlaylistSaveOptionsDialog::accept() {

  if (ui->remember_user_choice->isChecked()) {
    QSettings s;
    s.beginGroup(Playlist::kSettingsGroup);
    s.setValue(Playlist::kPathType, ui->filePaths->itemData(ui->filePaths->currentIndex()).toInt());
  }

  QDialog::accept();

}

Playlist::Path PlaylistSaveOptionsDialog::path_type() const {
  return static_cast<Playlist::Path>(ui->filePaths->itemData(ui->filePaths->currentIndex()).toInt());
}

