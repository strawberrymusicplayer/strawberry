/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QMimeDatabase>

#include "core/logging.h"
#include "tagreaderbase.h"

using namespace Qt::Literals::StringLiterals;

TagReaderBase::TagReaderBase() = default;
TagReaderBase::~TagReaderBase() = default;

float TagReaderBase::ConvertPOPMRating(const int POPM_rating) {

  if (POPM_rating < 0x01) return 0.0F;
  if (POPM_rating < 0x40) return 0.20F;
  if (POPM_rating < 0x80) return 0.40F;
  if (POPM_rating < 0xC0) return 0.60F;
  if (POPM_rating < 0xFC) return 0.80F;

  return 1.0F;

}

int TagReaderBase::ConvertToPOPMRating(const float rating) {

  if (rating < 0.20) return 0x00;
  if (rating < 0.40) return 0x01;
  if (rating < 0.60) return 0x40;
  if (rating < 0.80) return 0x80;
  if (rating < 1.0)  return 0xC0;

  return 0xFF;

}

TagReaderResult TagReaderBase::LoadAlbumCoverTagData(const QString &song_filename, const SaveTagCoverData &save_tag_cover_data, AlbumCoverTagData &cover) {

  // Empty cover data means the embedded cover is removed, so a cover that can't be loaded is an error, not empty data, otherwise the existing embedded cover is removed.
  cover = AlbumCoverTagData();

  const QString &cover_filename = save_tag_cover_data.cover_filename;
  QByteArray cover_data = save_tag_cover_data.cover_data;
  QString cover_mimetype = save_tag_cover_data.cover_mimetype;

  if (cover_data.isEmpty() && !cover_filename.isEmpty()) {
    qLog(Debug) << "Loading cover from" << cover_filename << "for" << song_filename;
    QFile file(cover_filename);
    if (!file.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Failed to open file" << cover_filename << "for reading:" << file.errorString();
      return TagReaderResult(TagReaderResult::ErrorCode::CustomError, QObject::tr("Could not open cover %1: %2").arg(cover_filename, file.errorString()));
    }
    cover_data = file.readAll();
    file.close();
    if (cover_data.isEmpty()) {
      qLog(Error) << "Cover" << cover_filename << "is empty";
      return TagReaderResult(TagReaderResult::ErrorCode::CustomError, QObject::tr("Cover %1 is empty").arg(cover_filename));
    }
  }

  if (!cover_data.isEmpty()) {
    if (cover_mimetype.isEmpty()) {
      cover_mimetype = QMimeDatabase().mimeTypeForData(cover_data).name();
    }
    // JPEG and PNG data is used as it is, but only when it can be decoded as that format, since the mimetype can be wrong and the data can be corrupt.
    QImage cover_image;
    if (cover_mimetype == "image/jpeg"_L1 || cover_mimetype == "image/png"_L1) {
      const char *format = cover_mimetype == "image/jpeg"_L1 ? "JPEG" : "PNG";
      if (cover_image.loadFromData(cover_data, format)) {
        qLog(Debug) << "Using cover from" << format << "data for" << song_filename;
        cover = AlbumCoverTagData(cover_data, cover_mimetype);
        return TagReaderResult::ErrorCode::Success;
      }
      qLog(Warning) << "Cover data for" << song_filename << "is not valid" << format << "data";
    }
    // Convert image to JPEG.
    qLog(Debug) << "Converting cover to JPEG data for" << song_filename;
    if (!cover_image.loadFromData(cover_data)) {
      qLog(Error) << "Failed to load image from cover data for" << song_filename;
      return TagReaderResult(TagReaderResult::ErrorCode::CustomError, QObject::tr("Could not load cover image"));
    }
    QByteArray jpeg_data;
    QBuffer buffer(&jpeg_data);
    if (!buffer.open(QIODevice::WriteOnly) || !cover_image.save(&buffer, "JPEG") || jpeg_data.isEmpty()) {
      qLog(Error) << "Failed to convert cover to JPEG for" << song_filename;
      return TagReaderResult(TagReaderResult::ErrorCode::CustomError, QObject::tr("Could not convert cover image to JPEG"));
    }
    buffer.close();
    cover = AlbumCoverTagData(jpeg_data, u"image/jpeg"_s);
  }

  return TagReaderResult::ErrorCode::Success;

}
