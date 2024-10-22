/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COVERFROMURLDIALOG_H
#define COVERFROMURLDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

#include "includes/shared_ptr.h"
#include "albumcoverimageresult.h"

class QWidget;

class NetworkAccessManager;
class Ui_CoverFromURLDialog;

// Controller for a dialog which fetches covers from the given URL.
class CoverFromURLDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CoverFromURLDialog(SharedPtr<NetworkAccessManager> network, QWidget *parent = nullptr);
  ~CoverFromURLDialog() override;

  // Opens the dialog. This returns an image found at the URL chosen by user or null image if the dialog got rejected.
  AlbumCoverImageResult Exec();

 private Q_SLOTS:
  void accept() override;
  void LoadCoverFromURLFinished();

 private:
  SharedPtr<NetworkAccessManager> network_;
  Ui_CoverFromURLDialog *ui_;
  AlbumCoverImageResult last_album_cover_;
};

#endif  // COVERFROMURLDIALOG_H
