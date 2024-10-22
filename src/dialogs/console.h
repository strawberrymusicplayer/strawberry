/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CONSOLE_H
#define CONSOLE_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QString>

#include "ui_console.h"

#include "includes/shared_ptr.h"

class Database;

class Console : public QDialog {
  Q_OBJECT

 public:
  explicit Console(const SharedPtr<Database> database, QWidget *parent = nullptr);

 private Q_SLOTS:
  void RunQuery();

 Q_SIGNALS:
  void Error(const QString &error);

 private:
  Ui::Console ui_;
  const SharedPtr<Database> database_;
};

#endif  // CONSOLE_H
