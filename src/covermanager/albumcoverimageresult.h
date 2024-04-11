/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ALBUMCOVERIMAGERESULT_H
#define ALBUMCOVERIMAGERESULT_H

#include <QMetaType>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>

class AlbumCoverImageResult {
 public:
  explicit AlbumCoverImageResult(const QUrl &_cover_url = QUrl(), const QString &_mime_type = QString(), const QByteArray &_image_data = QByteArray(), const QImage &_image = QImage())
    : cover_url(_cover_url),
      mime_type(_mime_type),
      image_data(_image_data),
      image(_image) {}
  explicit AlbumCoverImageResult(const QImage &_image) : image(_image) {}

  QUrl cover_url;
  QString mime_type;
  QByteArray image_data;
  QImage image;

  bool is_valid() const { return !image_data.isNull() || !image.isNull(); }
  bool is_jpeg() const { return mime_type == QStringLiteral("image/jpeg") && !image_data.isEmpty(); }
};

Q_DECLARE_METATYPE(AlbumCoverImageResult)

#endif  // ALBUMCOVERIMAGERESULT_H
