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

#ifndef ALBUMCOVEREXPORT_H
#define ALBUMCOVEREXPORT_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QString>

class QWidget;
class Ui_AlbumCoverExport;

// Controller for the "Export covers" dialog.
class AlbumCoverExport : public QDialog {
  Q_OBJECT

 public:
  AlbumCoverExport(QWidget *parent = nullptr);
  ~AlbumCoverExport();

  enum OverwriteMode {
    OverwriteMode_None = 0,
    OverwriteMode_All = 1,
    OverwriteMode_Smaller = 2
  };

  struct DialogResult {
    bool cancelled_;

    bool export_downloaded_;
    bool export_embedded_;

    QString fileName_;
    OverwriteMode overwrite_;
    bool forceSize_;
    int width_;
    int height_;

    bool IsSizeForced() const {
      return forceSize_ && width_ > 0 && height_ > 0;
    }

    bool RequiresCoverProcessing() const {
      return IsSizeForced() || overwrite_ == OverwriteMode_Smaller;
    }
  };

  DialogResult Exec();

 private slots:
  void ForceSizeToggled(int state);

 private:
  Ui_AlbumCoverExport *ui_;

  static const char *kSettingsGroup;
};

#endif  // ALBUMCOVEREXPORT_H

