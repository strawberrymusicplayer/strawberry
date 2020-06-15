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

#ifndef COVEREXPORTRUNNABLE_H
#define COVEREXPORTRUNNABLE_H

#include "config.h"

#include <QObject>
#include <QRunnable>
#include <QString>

#include "core/song.h"
#include "albumcoverexport.h"

class CoverExportRunnable : public QObject, public QRunnable {
  Q_OBJECT

 public:
  explicit CoverExportRunnable(const AlbumCoverExport::DialogResult &dialog_result, const Song &song);

  void run() override;

 signals:
  void CoverExported();
  void CoverSkipped();

 private:
  void EmitCoverExported();
  void EmitCoverSkipped();

  void ProcessAndExportCover();
  void ExportCover();
  QString GetCoverPath();

  AlbumCoverExport::DialogResult dialog_result_;
  Song song_;

};

#endif  // COVEREXPORTRUNNABLE_H

