/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QDialog>
#include <QString>
#include <QDir>
#include <QSettings>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

#include "saveplaylistsdialog.h"
#include "ui_saveplaylistsdialog.h"

#include "core/settings.h"
#include "constants/playlistsettings.h"

SavePlaylistsDialog::SavePlaylistsDialog(const QStringList &types, const QString &default_extension, QWidget *parent) : QDialog(parent), ui_(new Ui_SavePlaylistsDialog) {

  ui_->setupUi(this);

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  QString last_save_path = s.value(PlaylistSettings::kLastSaveAllPath, QDir::homePath()).toString();
  QString last_save_extension = s.value(PlaylistSettings::kLastSaveAllExtension, default_extension).toString();
  s.endGroup();

  ui_->lineedit_path->setText(last_save_path);
  ui_->combobox_type->addItems(types);

  int index = ui_->combobox_type->findText(last_save_extension);
  if (index >= 0) {
    ui_->combobox_type->setCurrentIndex(index);
  }

  QObject::connect(ui_->button_path, &QPushButton::clicked, this, &SavePlaylistsDialog::SelectPath);

}

SavePlaylistsDialog::~SavePlaylistsDialog() {
  delete ui_;
}

void SavePlaylistsDialog::SelectPath() {

  const QString path = QFileDialog::getExistingDirectory(nullptr, tr("Select directory for the playlists"),  ui_->lineedit_path->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (path.isEmpty()) return;

  ui_->lineedit_path->setText(path);

}

void SavePlaylistsDialog::accept() {

  const QString path = ui_->lineedit_path->text();
  if (path.isEmpty()) {
    return;
  }

  if (!QDir().exists(path)) {
    QMessageBox(QMessageBox::Warning, tr("Directory does not exist."), tr("Directory does not exist."), QMessageBox::Ok).exec();
    return;
  }

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  s.setValue(PlaylistSettings::kLastSaveAllPath, path);
  s.setValue(PlaylistSettings::kLastSaveAllExtension, ui_->combobox_type->currentText());
  s.endGroup();

  QDialog::accept();

}
