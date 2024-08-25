/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ALBUMCOVERLOADEROPTIONS_H
#define ALBUMCOVERLOADEROPTIONS_H

#include <QList>
#include <QImage>
#include <QSize>

class AlbumCoverLoaderOptions {
 public:
  enum class Option {
    NoOptions = 0x0,
    RawImageData = 0x2,
    OriginalImage = 0x4,
    ScaledImage = 0x8,
    PadScaledImage = 0x10
  };
  Q_DECLARE_FLAGS(Options, Option)

  enum class Type {
    Embedded,
    Automatic,
    Manual,
    Unset
  };
  using Types = QList<Type>;

  explicit AlbumCoverLoaderOptions(const Options _options = AlbumCoverLoaderOptions::Option::ScaledImage, const QSize _desired_scaled_size = QSize(32, 32), const qreal device_pixel_ratio = 1.0F, const Types &_types = QList<AlbumCoverLoaderOptions::Type>() << AlbumCoverLoaderOptions::Type::Embedded << AlbumCoverLoaderOptions::Type::Automatic << AlbumCoverLoaderOptions::Type::Manual);

  Options options;
  QSize desired_scaled_size;
  qreal device_pixel_ratio;
  Types types;
  QString default_cover;

  static Types LoadTypes();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AlbumCoverLoaderOptions::Options)

#endif  // ALBUMCOVERLOADEROPTIONS_H
