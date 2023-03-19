/* This file is part of Strawberry.
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <string>

#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QMimeDatabase>

#include "core/logging.h"
#include "tagreaderbase.h"

const std::string TagReaderBase::kEmbeddedCover = "(embedded)";

TagReaderBase::TagReaderBase() = default;
TagReaderBase::~TagReaderBase() = default;

float TagReaderBase::ConvertPOPMRating(const int POPM_rating) {

  if (POPM_rating < 0x01) return 0.0F;
  else if (POPM_rating < 0x40) return 0.20F;
  else if (POPM_rating < 0x80) return 0.40F;
  else if (POPM_rating < 0xC0) return 0.60F;
  else if (POPM_rating < 0xFC) return 0.80F;

  return 1.0F;

}

int TagReaderBase::ConvertToPOPMRating(const float rating) {

  if (rating < 0.20) return 0x00;
  else if (rating < 0.40) return 0x01;
  else if (rating < 0.60) return 0x40;
  else if (rating < 0.80) return 0x80;
  else if (rating < 1.0)  return 0xC0;

  return 0xFF;

}

QByteArray TagReaderBase::LoadCoverDataFromRequest(const spb::tagreader::SaveFileRequest &request) {

  if (!request.has_save_cover() || !request.save_cover()) {
    return QByteArray();
  }

  const QString song_filename = QString::fromUtf8(request.filename().data(), request.filename().size());
  QString cover_filename;
  if (request.has_cover_filename()) {
    cover_filename = QString::fromUtf8(request.cover_filename().data(), request.cover_filename().size());
  }
  QByteArray cover_data;
  if (request.has_cover_data()) {
    cover_data = QByteArray(request.cover_data().data(), request.cover_data().size());
  }
  bool cover_is_jpeg = false;
  if (request.has_cover_is_jpeg()) {
    cover_is_jpeg = request.cover_is_jpeg();
  }

  return LoadCoverDataFromRequest(song_filename, cover_filename, cover_data, cover_is_jpeg);

}

QByteArray TagReaderBase::LoadCoverDataFromRequest(const spb::tagreader::SaveEmbeddedArtRequest &request) {

  const QString song_filename = QString::fromUtf8(request.filename().data(), request.filename().size());
  QString cover_filename;
  if (request.has_cover_filename()) {
    cover_filename = QString::fromUtf8(request.cover_filename().data(), request.cover_filename().size());
  }
  QByteArray cover_data;
  if (request.has_cover_data()) {
    cover_data = QByteArray(request.cover_data().data(), request.cover_data().size());
  }
  bool cover_is_jpeg = false;
  if (request.has_cover_is_jpeg()) {
    cover_is_jpeg = request.cover_is_jpeg();
  }

  return LoadCoverDataFromRequest(song_filename, cover_filename, cover_data, cover_is_jpeg);

}

QByteArray TagReaderBase::LoadCoverDataFromRequest(const QString &song_filename, const QString &cover_filename, QByteArray cover_data, const bool cover_is_jpeg) {

  if (!cover_data.isEmpty() && cover_is_jpeg) {
    qLog(Debug) << "Using cover from JPEG data for" << song_filename;
    return cover_data;
  }

  if (cover_data.isEmpty() && !cover_filename.isEmpty()) {
    qLog(Debug) << "Loading cover from" << cover_filename << "for" << song_filename;
    QFile file(cover_filename);
    if (!file.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Failed to open file" << cover_filename << "for reading:" << file.errorString();
      return QByteArray();
    }
    cover_data = file.readAll();
    file.close();
  }

  if (!cover_data.isEmpty()) {
    if (QMimeDatabase().mimeTypeForData(cover_data).name() == "image/jpeg") {
      qLog(Debug) << "Using cover from JPEG data for" << song_filename;
      return cover_data;
    }
    // Convert image to JPEG.
    qLog(Debug) << "Converting cover to JPEG data for" << song_filename;
    QImage cover_image(cover_data);
    cover_data.clear();
    QBuffer buffer(&cover_data);
    if (buffer.open(QIODevice::WriteOnly)) {
      cover_image.save(&buffer, "JPEG");
      buffer.close();
    }
    return cover_data;
  }

  return QByteArray();

}
