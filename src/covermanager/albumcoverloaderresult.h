/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QMetaType>

#include "albumcoverimageresult.h"

class AlbumCoverLoaderResult {
 public:

  enum class Type {
    None,
    Unset,
    Embedded,
    Automatic,
    Manual
  };

  explicit AlbumCoverLoaderResult(const bool _success = false,
                                  const Type _type = Type::None,
                                  const AlbumCoverImageResult &_album_cover = AlbumCoverImageResult(),
                                  const QImage &_image_scaled = QImage(),
                                  const QUrl &_art_manual_updated = QUrl(),
                                  const QUrl &_art_automatic_updated = QUrl()) :
                                  success(_success),
                                  type(_type),
                                  album_cover(_album_cover),
                                  image_scaled(_image_scaled),
                                  art_manual_updated(_art_manual_updated),
                                  art_automatic_updated(_art_automatic_updated) {}

  bool success;
  Type type;
  AlbumCoverImageResult album_cover;
  QImage image_scaled;
  QUrl art_manual_updated;
  QUrl art_automatic_updated;

  QUrl temp_cover_url;
};

Q_DECLARE_METATYPE(AlbumCoverLoaderResult)

#endif  // ALBUMCOVERLOADERRESULT_H
