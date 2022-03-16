/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QImage>
#include <QSize>

struct AlbumCoverLoaderOptions {
  explicit AlbumCoverLoaderOptions()
      : get_image_data_(true),
        get_image_(true),
        scale_output_image_(true),
        pad_output_image_(true),
        create_thumbnail_(false),
        pad_thumbnail_image_(false),
        desired_height_(120),
        thumbnail_size_(120, 120) {}

  bool get_image_data_;
  bool get_image_;
  bool scale_output_image_;
  bool pad_output_image_;
  bool create_thumbnail_;
  bool pad_thumbnail_image_;
  int desired_height_;
  QSize thumbnail_size_;
  QImage default_output_image_;
  QImage default_scaled_image_;
  QImage default_thumbnail_image_;

};

#endif  // ALBUMCOVERLOADEROPTIONS_H
