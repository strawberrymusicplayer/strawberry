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

#ifndef PLAYLISTSAVEOPTIONSDIALOG_H
#define PLAYLISTSAVEOPTIONSDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

#include "constants/playlistsettings.h"

class QWidget;

namespace Ui {
class PlaylistSaveOptionsDialog;
}

class PlaylistSaveOptionsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit PlaylistSaveOptionsDialog(QWidget *parent = nullptr);
  ~PlaylistSaveOptionsDialog() override;

  void accept() override;
  PlaylistSettings::PathType path_type() const;

 private:
  static const char *kSettingsGroup;

  Ui::PlaylistSaveOptionsDialog *ui;
};

#endif  // PLAYLISTSAVEOPTIONSDIALOG_H
