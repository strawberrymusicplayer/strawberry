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

#include <utility>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSize>

#include "imageutils.h"
#include "fileutils.h"
#include "mimeutils.h"

using namespace Qt::Literals::StringLiterals;

QStringList ImageUtils::kSupportedImageMimeTypes;
QStringList ImageUtils::kSupportedImageFormats;

QStringList ImageUtils::SupportedImageMimeTypes() {

  if (kSupportedImageMimeTypes.isEmpty()) {
    const QList<QByteArray> supported_mimetypes = QImageReader::supportedMimeTypes();
    for (const QByteArray &mimetype : supported_mimetypes) {
      kSupportedImageMimeTypes << QString::fromUtf8(mimetype);
    }
  }

  return kSupportedImageMimeTypes;

}

QStringList ImageUtils::SupportedImageFormats() {

  if (kSupportedImageFormats.isEmpty()) {
    const QList<QByteArray> image_formats = QImageReader::supportedImageFormats();
    for (const QByteArray &filetype : image_formats) {
      kSupportedImageFormats << QString::fromUtf8(filetype);
    }
  }

  return kSupportedImageFormats;

}

QByteArray ImageUtils::SaveImageToJpegData(const QImage &image) {

  if (image.isNull()) return QByteArray();

  QByteArray image_data;
  QBuffer buffer(&image_data);
  if (buffer.open(QIODevice::WriteOnly)) {
    image.save(&buffer, "JPEG");
    buffer.close();
  }

  return image_data;

}

QByteArray ImageUtils::FileToJpegData(const QString &filename) {

  if (filename.isEmpty()) return QByteArray();

  QByteArray image_data = Utilities::ReadDataFromFile(filename);
  if (Utilities::MimeTypeFromData(image_data) == u"image/jpeg"_s) {
    return image_data;
  }

  QImage image;
  if (image.loadFromData(image_data)) {
    if (!image.isNull()) {
      image_data = SaveImageToJpegData(image);
    }
  }

  return image_data;

}

QImage ImageUtils::ScaleImage(const QImage &image, const QSize desired_size, const qreal device_pixel_ratio, const bool pad) {

  if (image.isNull() || (image.width() == desired_size.width() && image.height() == desired_size.height())) {
    return image;
  }

  QSize scale_size(static_cast<int>(desired_size.width() * device_pixel_ratio), static_cast<int>(desired_size.height() * device_pixel_ratio));

  // Scale the image
  QImage image_scaled = image.scaled(scale_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Pad the image
  if (pad && image_scaled.width() != image_scaled.height()) {
    QImage image_padded(scale_size, QImage::Format_ARGB32);
    image_padded.fill(0);

    QPainter p(&image_padded);
    p.drawImage((image_padded.width() - image_scaled.width()) / 2, (image_padded.height() - image_scaled.height()) / 2, image_scaled);
    p.end();

    image_scaled = image_padded;
  }

  image_scaled.setDevicePixelRatio(device_pixel_ratio);

  return image_scaled;

}

QImage ImageUtils::GenerateNoCoverImage(const QSize size, const qreal device_pixel_ratio) {

  QImage image(u":/pictures/cdcase.png"_s);
  QSize scale_size(static_cast<int>(size.width() * device_pixel_ratio), static_cast<int>(size.height() * device_pixel_ratio));

  // Get a square version of the nocover image with some transparency:
  QImage image_scaled = image.scaled(scale_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QImage image_square(scale_size, QImage::Format_ARGB32);
  image_square.fill(0);
  QPainter p(&image_square);
  p.setOpacity(0.4);
  p.drawImage((image_square.width() - image_scaled.width()) / 2, (image_square.height() - image_scaled.height()) / 2, image_scaled);
  p.end();

  image_square.setDevicePixelRatio(device_pixel_ratio);

  return image_square;

}
