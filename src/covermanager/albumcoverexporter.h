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

#ifndef ALBUMCOVEREXPORTER_H
#define ALBUMCOVEREXPORTER_H

#include "config.h"

#include <QObject>
#include <QQueue>
#include <QString>

#include "albumcoverexport.h"

class QThreadPool;
class Song;
class CoverExportRunnable;

class AlbumCoverExporter : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverExporter(QObject *parent = nullptr);
  virtual ~AlbumCoverExporter() {}

  static const int kMaxConcurrentRequests;

  void SetDialogResult(const AlbumCoverExport::DialogResult &dialog_result);
  void AddExportRequest(Song song);
  void StartExporting();
  void Cancel();

  int request_count() { return requests_.size(); }

signals:
  void AlbumCoversExportUpdate(int exported, int skipped, int all);

 private slots:
  void CoverExported();
  void CoverSkipped();

 private:
  void AddJobsToPool();
  AlbumCoverExport::DialogResult dialog_result_;

  QQueue<CoverExportRunnable*> requests_;
  QThreadPool *thread_pool_;

  int exported_;
  int skipped_;
  int all_;
};

#endif  // ALBUMCOVEREXPORTER_H

