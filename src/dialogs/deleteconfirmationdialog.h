/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DELETECONFIRMATIONDIALOG_H
#define DELETECONFIRMATIONDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QStringList>

class DeleteConfirmationDialog : public QDialog {
  Q_OBJECT

 public:
  DeleteConfirmationDialog(const QStringList &files, QWidget *parent = nullptr);

  static QDialogButtonBox::StandardButton warning(const QStringList &files, QWidget *parent = nullptr);

 private Q_SLOTS:
  void ButtonClicked(QAbstractButton *button);

 private:
  QDialogButtonBox *button_box_;
};

#endif  // DELETECONFIRMATIONDIALOG_H
