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

#include "core/settings.h"
#include "constants/playlistsettings.h"
#include "playlistsaveoptionsdialog.h"
#include "ui_playlistsaveoptionsdialog.h"

const char *PlaylistSaveOptionsDialog::kSettingsGroup = "PlaylistSaveOptionsDialog";

PlaylistSaveOptionsDialog::PlaylistSaveOptionsDialog(QWidget *parent) : QDialog(parent), ui(new Ui::PlaylistSaveOptionsDialog) {

  ui->setupUi(this);

  ui->filePaths->addItem(tr("Automatic"), QVariant::fromValue(PlaylistSettings::PathType::Automatic));
  ui->filePaths->addItem(tr("Relative"), QVariant::fromValue(PlaylistSettings::PathType::Relative));
  ui->filePaths->addItem(tr("Absolute"), QVariant::fromValue(PlaylistSettings::PathType::Absolute));

}

PlaylistSaveOptionsDialog::~PlaylistSaveOptionsDialog() { delete ui; }

void PlaylistSaveOptionsDialog::accept() {

  if (ui->remember_user_choice->isChecked()) {
    Settings s;
    s.beginGroup(PlaylistSettings::kSettingsGroup);
    s.setValue(PlaylistSettings::kPathType, ui->filePaths->itemData(ui->filePaths->currentIndex()).toInt());
    s.endGroup();
  }

  QDialog::accept();

}

PlaylistSettings::PathType PlaylistSaveOptionsDialog::path_type() const {
  return ui->filePaths->itemData(ui->filePaths->currentIndex()).value<PlaylistSettings::PathType>();
}
