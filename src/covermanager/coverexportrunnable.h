/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverexport.h"

class QImage;
class TagReaderClient;

class CoverExportRunnable : public QObject, public QRunnable {
  Q_OBJECT

 public:
  explicit CoverExportRunnable(const SharedPtr<TagReaderClient> tagreader_client,
                               const AlbumCoverExport::DialogResult &dialog_result,
                               const AlbumCoverLoaderOptions::Types &cover_types,
                               const Song &song,
                               QObject *parent = nullptr);

  void run() override;

 Q_SIGNALS:
  void CoverExported();
  void CoverSkipped();

 private:
  void EmitCoverExported();
  void EmitCoverSkipped();

  void ProcessAndExportCover();
  void ExportCover();
  static bool SaveImage(const QImage &image, const QString &filename);
  static bool CopyCoverFile(const QString &source_filename, const QString &destination_filename);

  SharedPtr<TagReaderClient> tagreader_client_;
  AlbumCoverExport::DialogResult dialog_result_;
  AlbumCoverLoaderOptions::Types cover_types_;
  Song song_;
};

#endif  // COVEREXPORTRUNNABLE_H
