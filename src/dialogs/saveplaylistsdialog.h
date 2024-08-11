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

#ifndef SAVEPLAYLISTSDIALOG_H
#define SAVEPLAYLISTSDIALOG_H

#include <QDialog>
#include <QString>
#include <QLineEdit>
#include <QComboBox>

#include "ui_saveplaylistsdialog.h"

class SavePlaylistsDialog : public QDialog {
 Q_OBJECT

 public:
  explicit SavePlaylistsDialog(const QStringList &types, const QString &default_extension, QWidget *parent = nullptr);
  ~SavePlaylistsDialog();

  QString path() const { return ui_->lineedit_path->text(); }
  QString extension() const { return ui_->combobox_type->currentText(); };

 protected:
  void accept() override;

 private Q_SLOTS:
  void SelectPath();

 private:
  Ui_SavePlaylistsDialog *ui_;
};

#endif  // SAVEPLAYLISTSDIALOG_H
