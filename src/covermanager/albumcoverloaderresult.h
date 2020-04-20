/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ALBUMCOVERLOADERRESULT_H
#define ALBUMCOVERLOADERRESULT_H

#include "config.h"

#include <QImage>
#include <QUrl>

struct AlbumCoverLoaderResult {

  enum Type {
    Type_None,
    Type_ManuallyUnset,
    Type_Embedded,
    Type_Automatic,
    Type_Manual,
    Type_Remote,
  };

  explicit AlbumCoverLoaderResult(const Type _type = Type_None, const QUrl &_cover_url = QUrl(), const QImage &_image_original = QImage(), const QImage &_image_scaled = QImage(), const QImage &_image_thumbnail = QImage(), const bool _updated = false) : type(_type), cover_url(_cover_url), image_original(_image_original), image_scaled(_image_scaled), image_thumbnail(_image_thumbnail), updated(_updated) {}

  Type type;
  QUrl cover_url;
  QImage image_original;
  QImage image_scaled;
  QImage image_thumbnail;
  bool updated;

  QUrl temp_cover_url;

};

#endif  // ALBUMCOVERLOADERRESULT_H
