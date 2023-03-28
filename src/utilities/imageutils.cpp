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

#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QPainter>
#include <QSize>

#include "imageutils.h"
#include "fileutils.h"
#include "mimeutils.h"
#include "core/tagreaderclient.h"

QStringList ImageUtils::kSupportedImageMimeTypes;
QStringList ImageUtils::kSupportedImageFormats;

QStringList ImageUtils::SupportedImageMimeTypes() {

  if (kSupportedImageMimeTypes.isEmpty()) {
    for (const QByteArray &mimetype : QImageReader::supportedMimeTypes()) {
      kSupportedImageMimeTypes << mimetype;
    }
  }

  return kSupportedImageMimeTypes;

}

QStringList ImageUtils::SupportedImageFormats() {

  if (kSupportedImageFormats.isEmpty()) {
    for (const QByteArray &filetype : QImageReader::supportedImageFormats()) {
      kSupportedImageFormats << filetype;
    }
  }

  return kSupportedImageFormats;

}

QByteArrayList ImageUtils::ImageFormatsForMimeType(const QByteArray &mimetype) {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
  return QImageReader::imageFormatsForMimeType(mimetype);
#else
  if (mimetype == "image/bmp") return QByteArrayList() << "BMP";
  else if (mimetype == "image/gif") return QByteArrayList() << "GIF";
  else if (mimetype == "image/jpeg") return QByteArrayList() << "JPG";
  else if (mimetype == "image/png") return QByteArrayList() << "PNG";
  else return QByteArrayList();
#endif

}

QPixmap ImageUtils::TryLoadPixmap(const QUrl &art_automatic, const QUrl &art_manual, const QUrl &url) {

  QPixmap ret;

  if (!art_manual.path().isEmpty()) {
    if (art_manual.path() == Song::kManuallyUnsetCover) return ret;
    else if (art_manual.isLocalFile()) {
      ret.load(art_manual.toLocalFile());
    }
    else if (art_manual.scheme().isEmpty()) {
      ret.load(art_manual.path());
    }
  }
  if (ret.isNull() && !art_automatic.path().isEmpty()) {
    if (art_automatic.path() == Song::kEmbeddedCover && !url.isEmpty() && url.isLocalFile()) {
      ret = QPixmap::fromImage(TagReaderClient::Instance()->LoadEmbeddedArtAsImageBlocking(url.toLocalFile()));
    }
    else if (art_automatic.isLocalFile()) {
      ret.load(art_automatic.toLocalFile());
    }
    else if (art_automatic.scheme().isEmpty()) {
      ret.load(art_automatic.path());
    }
  }

  return ret;

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
  if (Utilities::MimeTypeFromData(image_data) == "image/jpeg") return image_data;
  else {
    QImage image;
    if (image.loadFromData(image_data)) {
      if (!image.isNull()) {
        image_data = SaveImageToJpegData(image);
      }
    }
  }

  return image_data;

}

QImage ImageUtils::ScaleAndPad(const QImage &image, const bool scale, const bool pad, const int desired_height, const qreal device_pixel_ratio) {

  if (image.isNull()) return image;

  const int scaled_height = desired_height * device_pixel_ratio;

  // Scale the image down
  QImage image_scaled;
  if (scale) {
    image_scaled = image.scaled(QSize(scaled_height, scaled_height), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  else {
    image_scaled = image;
  }

  // Pad the image to height x height
  if (pad) {
    QImage image_padded(scaled_height, scaled_height, QImage::Format_ARGB32);
    image_padded.fill(0);

    QPainter p(&image_padded);
    p.drawImage((scaled_height - image_scaled.width()) / 2, (scaled_height - image_scaled.height()) / 2, image_scaled);
    p.end();

    image_scaled = image_padded;
  }

  image_scaled.setDevicePixelRatio(device_pixel_ratio);

  return image_scaled;

}

QImage ImageUtils::CreateThumbnail(const QImage &image, const bool pad, const QSize size) {

  if (image.isNull()) return image;

  QImage image_thumbnail;
  if (pad) {
    image_thumbnail = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage image_padded(size, QImage::Format_ARGB32_Premultiplied);
    image_padded.fill(0);

    QPainter p(&image_padded);
    p.drawImage((image_padded.width() - image_thumbnail.width()) / 2, (image_padded.height() - image_thumbnail.height()) / 2, image_thumbnail);
    p.end();

    image_thumbnail = image_padded;
  }
  else {
    image_thumbnail = image.scaledToHeight(size.height(), Qt::SmoothTransformation);
  }

  return image_thumbnail;

}

QImage ImageUtils::GenerateNoCoverImage(const QSize size) {

  QImage image(":/pictures/cdcase.png");

  // Get a square version of the nocover image with some transparency:
  QImage image_scaled = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QImage image_square(size, QImage::Format_ARGB32);
  image_square.fill(0);
  QPainter p(&image_square);
  p.setOpacity(0.4);
  p.drawImage((size.width() - image_scaled.width()) / 2, (size.height() - image_scaled.height()) / 2, image_scaled);
  p.end();

  return image_square;

}
