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

#include "settings/playlistsettingspage.h"
#include "playlistsaveoptionsdialog.h"
#include "ui_playlistsaveoptionsdialog.h"

const char *PlaylistSaveOptionsDialog::kSettingsGroup = "PlaylistSaveOptionsDialog";

PlaylistSaveOptionsDialog::PlaylistSaveOptionsDialog(QWidget *parent) : QDialog(parent), ui(new Ui::PlaylistSaveOptionsDialog) {

  ui->setupUi(this);

  ui->filePaths->addItem(tr("Automatic"), QVariant::fromValue(PlaylistSettingsPage::PathType::Automatic));
  ui->filePaths->addItem(tr("Relative"), QVariant::fromValue(PlaylistSettingsPage::PathType::Relative));
  ui->filePaths->addItem(tr("Absolute"), QVariant::fromValue(PlaylistSettingsPage::PathType::Absolute));

}

PlaylistSaveOptionsDialog::~PlaylistSaveOptionsDialog() { delete ui; }

void PlaylistSaveOptionsDialog::accept() {

  if (ui->remember_user_choice->isChecked()) {
    QSettings s;
    s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
    s.setValue("path_type", ui->filePaths->itemData(ui->filePaths->currentIndex()).toInt());
    s.endGroup();
  }

  QDialog::accept();

}

PlaylistSettingsPage::PathType PlaylistSaveOptionsDialog::path_type() const {
  return ui->filePaths->itemData(ui->filePaths->currentIndex()).value<PlaylistSettingsPage::PathType>();
}
