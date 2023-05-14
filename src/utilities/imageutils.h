/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QPixmap>

class ImageUtils {

 private:
  static QStringList kSupportedImageMimeTypes;
  static QStringList kSupportedImageFormats;

 public:
  static QStringList SupportedImageMimeTypes();
  static QStringList SupportedImageFormats();
  static QByteArray SaveImageToJpegData(const QImage &image = QImage());
  static QByteArray FileToJpegData(const QString &filename);
  static QImage ScaleImage(const QImage &image, const QSize desired_size, const qreal device_pixel_ratio = 1.0F, const bool pad = true);
  static QImage GenerateNoCoverImage(const QSize size, const qreal device_pixel_ratio);
};

#endif  // IMAGEUTILS_H
