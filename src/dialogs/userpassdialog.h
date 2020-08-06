/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef USERPASSDIALOG_H
#define USERPASSDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

#include "ui_userpassdialog.h"

class UserPassDialog : public QDialog {
  Q_OBJECT

 public:
  explicit UserPassDialog(QWidget *parent = nullptr);
  ~UserPassDialog() override;

  QString username() const { return ui_->username->text(); }
  QString password() const { return ui_->password->text(); }

 private:
  Ui_UserPassDialog *ui_;
};

#endif  // USERPASSDIALOG_H
