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

#include "config.h"

#include <QObject>
#include <QThreadPool>

#include "core/song.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverexport.h"
#include "albumcoverexporter.h"
#include "coverexportrunnable.h"

namespace {
constexpr int kMaxConcurrentRequests = 3;
}

AlbumCoverExporter::AlbumCoverExporter(const SharedPtr<TagReaderClient> tagreader_client, QObject *parent)
    : QObject(parent),
      tagreader_client_(tagreader_client),
      thread_pool_(new QThreadPool(this)),
      export_id_(0),
      running_(0),
      exported_(0),
      skipped_(0),
      all_(0) {
  thread_pool_->setMaxThreadCount(kMaxConcurrentRequests);
}

void AlbumCoverExporter::SetDialogResult(const AlbumCoverExport::DialogResult &dialog_result) {
  dialog_result_ = dialog_result;
}

void AlbumCoverExporter::SetCoverTypes(const AlbumCoverLoaderOptions::Types &cover_types) {
  cover_types_ = cover_types;
}

void AlbumCoverExporter::AddExportRequest(const Song &song) {

  requests_.append(new CoverExportRunnable(tagreader_client_, dialog_result_, cover_types_, song));
  all_ = static_cast<int>(requests_.count());

}

void AlbumCoverExporter::Cancel() {

  // The queued runnables haven't been handed to the thread pool yet, so delete them ourselves.
  qDeleteAll(requests_);
  requests_.clear();

  // The covers being exported can't be stopped, they're not counted when they finish, so they don't update the progress after aborting, or count for the next export.
  ++export_id_;

}

void AlbumCoverExporter::StartExporting() {

  exported_ = 0;
  skipped_ = 0;
  AddJobsToPool();

}

void AlbumCoverExporter::AddJobsToPool() {

  // The covers being exported are counted here, the thread pool can still count a thread as active when the cover it exported is received, then no more covers would be exported.
  while (!requests_.isEmpty() && running_ < kMaxConcurrentRequests) {
    CoverExportRunnable *runnable = requests_.dequeue();

    const int export_id = export_id_;
    QObject::connect(runnable, &CoverExportRunnable::CoverExported, this, [this, export_id]() { CoverExported(export_id); });
    QObject::connect(runnable, &CoverExportRunnable::CoverSkipped, this, [this, export_id]() { CoverSkipped(export_id); });

    ++running_;
    thread_pool_->start(runnable);
  }

}

void AlbumCoverExporter::CoverExported(const int export_id) {

  if (export_id == export_id_) {
    ++exported_;
    Q_EMIT AlbumCoversExportUpdate(exported_, skipped_, all_);
  }

  CoverFinished();

}

void AlbumCoverExporter::CoverSkipped(const int export_id) {

  if (export_id == export_id_) {
    ++skipped_;
    Q_EMIT AlbumCoversExportUpdate(exported_, skipped_, all_);
  }

  CoverFinished();

}

void AlbumCoverExporter::CoverFinished() {

  --running_;
  AddJobsToPool();

}
