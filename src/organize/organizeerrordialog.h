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

#ifndef ORGANISEERRORDIALOG_H
#define ORGANISEERRORDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QWidget>
#include <QString>
#include <QStringList>

#include "core/song.h"

class Ui_OrganizeErrorDialog;

class OrganizeErrorDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OrganizeErrorDialog(QWidget *parent = nullptr);
  ~OrganizeErrorDialog() override;

  enum class OperationType {
    Copy,
    Delete
  };

  void Show(const OperationType operation_type, const SongList &songs_with_errors, const QStringList &log = QStringList());
  void Show(const OperationType operation_type, const QStringList &files_with_errors, const QStringList &log = QStringList());

 private:
  Ui_OrganizeErrorDialog *ui_;
};

#endif  // ORGANISEERRORDIALOG_H
