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

#ifndef ERRORDIALOG_H
#define ERRORDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QWidget>
#include <QString>
#include <QStringList>

class QCloseEvent;
class Ui_ErrorDialog;

class ErrorDialog : public QDialog {
  Q_OBJECT

 public:
  explicit ErrorDialog(QWidget *parent = nullptr);
  ~ErrorDialog() override;

 public Q_SLOTS:
  void ShowMessage(const QString &message);

 protected:
  void closeEvent(QCloseEvent *e) override;

 private:
  void UpdateContent();

  QWidget *parent_;
  Ui_ErrorDialog *ui_;

  QStringList current_messages_;
};

#endif  // ERRORDIALOG_H
