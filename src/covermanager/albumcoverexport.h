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
  explicit AlbumCoverExport(QWidget *parent = nullptr);
  ~AlbumCoverExport() override;

  enum class OverwriteMode {
    None = 0,
    All = 1,
    Smaller = 2
  };

  struct DialogResult {
    DialogResult() : cancelled_(false), export_downloaded_(false), export_embedded_(false), forcesize_(false), width_(0), height_(0) {}
    bool cancelled_;

    bool export_downloaded_;
    bool export_embedded_;

    QString filename_;
    OverwriteMode overwrite_;
    bool forcesize_;
    int width_;
    int height_;

    bool IsSizeForced() const {
      return forcesize_ && width_ > 0 && height_ > 0;
    }

    bool RequiresCoverProcessing() const {
      return IsSizeForced() || overwrite_ == OverwriteMode::Smaller;
    }
  };

  DialogResult Exec();

 private Q_SLOTS:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  void ForceSizeToggled(Qt::CheckState state);
#else
  void ForceSizeToggled(int state);
#endif

 private:
  Ui_AlbumCoverExport *ui_;
};

#endif  // ALBUMCOVEREXPORT_H
