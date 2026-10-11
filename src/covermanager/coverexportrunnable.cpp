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

#include <utility>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSize>
#include <QString>
#include <QImage>

#include "core/logging.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverexport.h"
#include "coverexportrunnable.h"

using namespace Qt::Literals::StringLiterals;

CoverExportRunnable::CoverExportRunnable(const SharedPtr<TagReaderClient> tagreader_client, const AlbumCoverExport::DialogResult &dialog_result, const AlbumCoverLoaderOptions::Types &cover_types, const Song &song, QObject *parent)
    : QObject(parent),
      tagreader_client_(tagreader_client),
      dialog_result_(dialog_result),
      cover_types_(cover_types),
      song_(song) {}

void CoverExportRunnable::run() {

  if (song_.art_unset() || (!song_.art_embedded() && !song_.art_automatic_is_valid() && !song_.art_manual_is_valid())) {
    EmitCoverSkipped();
  }
  else {
    if (dialog_result_.RequiresCoverProcessing()) {
      ProcessAndExportCover();
    }
    else {
      ExportCover();
    }
  }

}

// Exports a single album cover using a "save QImage to file" approach.
// For performance reasons this method will be invoked only if loading and in memory processing of images is necessary for current settings which means that:
// - either the force size flag is being used
// - or the "overwrite smaller" mode is used
// In all other cases, the faster ExportCover() method will be used.
void CoverExportRunnable::ProcessAndExportCover() {

  QImage image;
  QString extension;

  for (const AlbumCoverLoaderOptions::Type cover_type : std::as_const(cover_types_)) {
    switch (cover_type) {
      case AlbumCoverLoaderOptions::Type::Unset:
        if (song_.art_unset()) {
          EmitCoverSkipped();
          return;
        }
        break;
      case AlbumCoverLoaderOptions::Type::Embedded:
        if (song_.art_embedded() && dialog_result_.export_embedded_) {
          const TagReaderResult result = tagreader_client_->LoadCoverImageBlocking(song_.url().toLocalFile(), image);
          if (result.success() && !image.isNull()) {
            extension = "jpg"_L1;
          }
        }
        break;
      case AlbumCoverLoaderOptions::Type::Manual:
        if (dialog_result_.export_downloaded_ && song_.art_manual_is_valid()) {
          const QString cover_path = song_.art_manual().toLocalFile();
          if (image.load(cover_path)) {
            extension = cover_path.section(u'.', -1);
          }
        }
        break;
      case AlbumCoverLoaderOptions::Type::Automatic:
        if (dialog_result_.export_downloaded_ && song_.art_automatic_is_valid()) {
          const QString cover_path = song_.art_automatic().toLocalFile();
          if (image.load(cover_path)) {
            extension = cover_path.section(u'.', -1);
          }
        }
        break;
    }
    if (!image.isNull() && !extension.isEmpty()) break;
  }

  if (image.isNull() || extension.isEmpty()) {
    EmitCoverSkipped();
    return;
  }

  // Rescale if necessary
  if (dialog_result_.IsSizeForced()) {
    image = image.scaled(QSize(dialog_result_.width_, dialog_result_.height_), Qt::IgnoreAspectRatio);
  }

  QString cover_dir = song_.url().toLocalFile().section(u'/', 0, -2);
  QString new_file = cover_dir + QLatin1Char('/') + dialog_result_.filename_ + QLatin1Char('.') + (song_.art_embedded() ? "jpg"_L1 : extension);

  // If the file exists, do not override!
  if (dialog_result_.overwrite_ == AlbumCoverExport::OverwriteMode::None && QFile::exists(new_file)) {
    EmitCoverSkipped();
    return;
  }

  // If the mode is "overwrite smaller" then skip the cover if a bigger one is already available in the folder
  if (dialog_result_.overwrite_ == AlbumCoverExport::OverwriteMode::Smaller && QFile::exists(new_file)) {
    QImage image_existing;
    image_existing.load(new_file);

    if (image_existing.isNull() || image_existing.size().height() >= image.size().height() || image_existing.size().width() >= image.size().width()) {
      EmitCoverSkipped();
      return;
    }
  }

  // The existing file is only replaced when the new one is written, so it's not lost if writing fails.
  if (SaveImage(image, new_file)) {
    EmitCoverExported();
  }
  else {
    EmitCoverSkipped();
  }

}

// Exports a single album cover using a "copy file" approach.
void CoverExportRunnable::ExportCover() {

  QImage image;
  QString extension;
  QString cover_path;
  bool embedded_cover = false;

  for (const AlbumCoverLoaderOptions::Type cover_type : std::as_const(cover_types_)) {
    switch (cover_type) {
      case AlbumCoverLoaderOptions::Type::Unset:
        if (song_.art_unset()) {
          EmitCoverSkipped();
          return;
        }
        break;
      case AlbumCoverLoaderOptions::Type::Embedded:
        if (song_.art_embedded() && dialog_result_.export_embedded_) {
          const TagReaderResult result = tagreader_client_->LoadCoverImageBlocking(song_.url().toLocalFile(), image);
          if (result.success() && !image.isNull()) {
            embedded_cover = true;
            extension = "jpg"_L1;
          }
        }
        break;
      case AlbumCoverLoaderOptions::Type::Manual:
        if (dialog_result_.export_downloaded_ && song_.art_manual_is_valid()) {
          cover_path = song_.art_manual().toLocalFile();
          if (image.load(cover_path)) {
            extension = cover_path.section(u'.', -1);
          }
        }
        break;
      case AlbumCoverLoaderOptions::Type::Automatic:
        if (dialog_result_.export_downloaded_ && song_.art_automatic_is_valid()) {
          cover_path = song_.art_automatic().toLocalFile();
          if (image.load(cover_path)) {
            extension = cover_path.section(u'.', -1);
          }
        }
        break;
    }
    if (!image.isNull() && !extension.isEmpty() && (embedded_cover || !cover_path.isEmpty())) break;
  }

  if (image.isNull() || extension.isEmpty()) {
    EmitCoverSkipped();
    return;
  }

  QString cover_dir = song_.url().toLocalFile().section(u'/', 0, -2);
  QString new_file = cover_dir + QLatin1Char('/') + dialog_result_.filename_ + QLatin1Char('.') + extension;

  // If the file exists, do not override!
  if (dialog_result_.overwrite_ == AlbumCoverExport::OverwriteMode::None && QFile::exists(new_file)) {
    EmitCoverSkipped();
    return;
  }

  // The cover is already the file it would be exported to, which is common since the default filename is "cover".
  // Overwriting it with itself would at best do nothing.
  if (!embedded_cover && QFile::exists(new_file) && QFileInfo(cover_path).canonicalFilePath() == QFileInfo(new_file).canonicalFilePath()) {
    EmitCoverSkipped();
    return;
  }

  // The existing file is only replaced when the new one is written, so it's not lost if writing fails.
  if (embedded_cover) {
    if (!SaveImage(image, new_file)) {
      EmitCoverSkipped();
      return;
    }
  }
  else {
    // Automatic or manual cover, available in an image file
    if (!CopyCoverFile(cover_path, new_file)) {
      EmitCoverSkipped();
      return;
    }
  }

  EmitCoverExported();

}

bool CoverExportRunnable::SaveImage(const QImage &image, const QString &filename) {

  QSaveFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qLog(Error) << "Failed to open" << filename << "for writing:" << file.errorString();
    return false;
  }

  // QImage can't guess the format from the filename when saving to a device.
  const QByteArray format = QFileInfo(filename).suffix().toLower().toLatin1();
  if (!image.save(&file, format.constData()) || !file.commit()) {
    qLog(Error) << "Failed to save cover to" << filename << file.errorString();
    return false;
  }

  return true;

}

bool CoverExportRunnable::CopyCoverFile(const QString &source_filename, const QString &destination_filename) {

  QFile source_file(source_filename);
  if (!source_file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Failed to open" << source_filename << "for reading:" << source_file.errorString();
    return false;
  }

  QSaveFile destination_file(destination_filename);
  if (!destination_file.open(QIODevice::WriteOnly)) {
    qLog(Error) << "Failed to open" << destination_filename << "for writing:" << destination_file.errorString();
    return false;
  }

  // Copy in chunks, so the whole cover is not kept in memory while the decoded image is too.
  // Returning before commit() discards the new file, so the existing file is only replaced when the whole cover is copied.
  constexpr qint64 kChunkSize = 64 * 1024;
  while (true) {
    const QByteArray chunk = source_file.read(kChunkSize);
    if (chunk.isEmpty()) break;
    // A short write is a failure too.
    if (destination_file.write(chunk) != chunk.size()) {
      qLog(Error) << "Failed to write to" << destination_filename << ":" << destination_file.errorString();
      return false;
    }
  }

  // read() returns an empty chunk on errors too, so check the source, since replacing the existing file with a partial cover would lose it.
  if (source_file.error() != QFileDevice::NoError) {
    qLog(Error) << "Failed to read from" << source_filename << ":" << source_file.errorString();
    return false;
  }

  if (!destination_file.commit()) {
    qLog(Error) << "Failed to copy" << source_filename << "to" << destination_filename << destination_file.errorString();
    return false;
  }

  return true;

}

void CoverExportRunnable::EmitCoverExported() { Q_EMIT CoverExported(); }

void CoverExportRunnable::EmitCoverSkipped() { Q_EMIT CoverSkipped(); }
