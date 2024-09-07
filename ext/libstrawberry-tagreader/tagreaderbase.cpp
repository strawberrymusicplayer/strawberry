/* This file is part of Strawberry.
   Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>

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

using namespace Qt::StringLiterals;

TagReaderBase::TagReaderBase() = default;
TagReaderBase::~TagReaderBase() = default;

QString TagReaderBase::ErrorString(const Result &result) {

  switch (result.error_code) {
    case Result::ErrorCode::Success:
      return QObject::tr("Success");
    case Result::ErrorCode::Unsupported:
      return QObject::tr("File is unsupported");
    case Result::ErrorCode::FilenameMissing:
      return QObject::tr("Filename is missing");
    case Result::ErrorCode::FileDoesNotExist:
      return QObject::tr("File does not exist");
    case Result::ErrorCode::FileOpenError:
      return QObject::tr("File could not be opened");
    case Result::ErrorCode::FileParseError:
      return QObject::tr("Could not parse file");
    case Result::ErrorCode::FileSaveError:
      return QObject::tr("Could save file");
    case Result::ErrorCode::CustomError:
      return result.error;
  }

  return QObject::tr("Unknown error");

}

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

TagReaderBase::Cover TagReaderBase::LoadCoverFromRequest(const QString &song_filename, const spb::tagreader::WriteFileRequest &request) {

  if (!request.has_save_cover() || !request.save_cover()) {
    return Cover();
  }

  QString cover_filename;
  if (request.has_cover_filename()) {
    cover_filename = QString::fromStdString(request.cover_filename());
  }
  QByteArray cover_data;
  if (request.has_cover_data()) {
    cover_data = QByteArray(request.cover_data().data(), static_cast<qint64>(request.cover_data().size()));
  }
  QString cover_mime_type;
  if (request.has_cover_mime_type()) {
    cover_mime_type = QString::fromStdString(request.cover_mime_type());
  }

  return LoadCoverFromRequest(song_filename, cover_filename, cover_data, cover_mime_type);

}

TagReaderBase::Cover TagReaderBase::LoadCoverFromRequest(const QString &song_filename, const spb::tagreader::SaveEmbeddedArtRequest &request) {

  QString cover_filename;
  if (request.has_cover_filename()) {
    cover_filename = QString::fromStdString(request.cover_filename());
  }
  QByteArray cover_data;
  if (request.has_cover_data()) {
    cover_data = QByteArray(request.cover_data().data(), static_cast<qint64>(request.cover_data().size()));
  }
  QString cover_mime_type;
  if (request.has_cover_mime_type()) {
    cover_mime_type = QString::fromStdString(request.cover_mime_type());
  }

  return LoadCoverFromRequest(song_filename, cover_filename, cover_data, cover_mime_type);

}

TagReaderBase::Cover TagReaderBase::LoadCoverFromRequest(const QString &song_filename, const QString &cover_filename, QByteArray cover_data, QString cover_mime_type) {

  if (cover_data.isEmpty() && !cover_filename.isEmpty()) {
    qLog(Debug) << "Loading cover from" << cover_filename << "for" << song_filename;
    QFile file(cover_filename);
    if (!file.open(QIODevice::ReadOnly)) {
      qLog(Error) << "Failed to open file" << cover_filename << "for reading:" << file.errorString();
      return Cover();
    }
    cover_data = file.readAll();
    file.close();
  }

  if (!cover_data.isEmpty()) {
    if (cover_mime_type.isEmpty()) {
      cover_mime_type = QMimeDatabase().mimeTypeForData(cover_data).name();
    }
    if (cover_mime_type == "image/jpeg"_L1) {
      qLog(Debug) << "Using cover from JPEG data for" << song_filename;
      return Cover(cover_data, cover_mime_type);
    }
    if (cover_mime_type == "image/png"_L1) {
      qLog(Debug) << "Using cover from PNG data for" << song_filename;
      return Cover(cover_data, cover_mime_type);
    }
    // Convert image to JPEG.
    qLog(Debug) << "Converting cover to JPEG data for" << song_filename;
    QImage cover_image;
    if (!cover_image.loadFromData(cover_data)) {
      qLog(Error) << "Failed to load image from cover data for" << song_filename;
      return Cover();
    }
    cover_data.clear();
    QBuffer buffer(&cover_data);
    if (buffer.open(QIODevice::WriteOnly)) {
      cover_image.save(&buffer, "JPEG");
      buffer.close();
    }
    return Cover(cover_data, QStringLiteral("image/jpeg"));
  }

  return Cover();

}
