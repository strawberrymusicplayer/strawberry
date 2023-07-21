/*
 * Strawberry Music Player
 * Copyright 2020-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SNAPDIALOG_H
#define SNAPDIALOG_H

#include "messagedialog.h"

class SnapDialog : public MessageDialog {
  Q_OBJECT

 public:
  explicit SnapDialog(QWidget *parent = nullptr);
};

#endif  // SNAPDIALOG_H
