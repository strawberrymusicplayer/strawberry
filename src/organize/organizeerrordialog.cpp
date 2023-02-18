/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QWidget>
#include <QDialog>
#include <QIcon>
#include <QStyle>
#include <QList>
#include <QStringList>
#include <QLabel>
#include <QListWidget>

#include "organizeerrordialog.h"
#include "ui_organizeerrordialog.h"

OrganizeErrorDialog::OrganizeErrorDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_OrganizeErrorDialog) {

  ui_->setupUi(this);

  const int icon_size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
  QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxCritical, nullptr, this);

  ui_->icon->setPixmap(icon.pixmap(icon_size));

}

OrganizeErrorDialog::~OrganizeErrorDialog() {
  delete ui_;
}

void OrganizeErrorDialog::Show(const OperationType operation_type, const SongList &songs_with_errors, const QStringList &log) {

  QStringList files;
  files.reserve(songs_with_errors.count());
  for (const Song &song : songs_with_errors) {
    files << song.url().toLocalFile();
  }
  Show(operation_type, files, log);

}

void OrganizeErrorDialog::Show(const OperationType operation_type, const QStringList &files_with_errors, const QStringList &log) {

  QStringList sorted_files = files_with_errors;
  std::stable_sort(sorted_files.begin(), sorted_files.end());

  switch (operation_type) {
    case OperationType::Copy:
      setWindowTitle(tr("Error copying songs"));
      ui_->label->setText(tr("There were problems copying some songs.  The following files could not be copied:"));
      break;

    case OperationType::Delete:
      setWindowTitle(tr("Error deleting songs"));
      ui_->label->setText(tr("There were problems deleting some songs.  The following files could not be deleted:"));
      break;
  }

  ui_->files->addItems(sorted_files);
  ui_->log->addItems(log);

  show();

}
