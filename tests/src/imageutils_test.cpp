/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "gtest_include.h"

#include <QColor>
#include <QImage>
#include <QSize>

#include "utilities/imageutils.h"

namespace {

QImage MakeImage(const int width, const int height) {
  QImage image(width, height, QImage::Format_ARGB32);
  image.fill(Qt::red);
  return image;
}

// Regression test: an image with the size asked for was returned as it was, so the device pixel ratio was not applied.
TEST(ImageUtilsTest, ScaleImageAppliesDevicePixelRatioToImageWithDesiredSize) {

  const QImage image = ImageUtils::ScaleImage(MakeImage(32, 32), QSize(32, 32), 2.0, false);

  EXPECT_EQ(QSize(64, 64), image.size());
  EXPECT_EQ(2.0, image.devicePixelRatio());

}

TEST(ImageUtilsTest, ScaleImageAppliesDevicePixelRatioToImageWithScaledSize) {

  const QImage image = ImageUtils::ScaleImage(MakeImage(64, 64), QSize(32, 32), 2.0, false);

  EXPECT_EQ(QSize(64, 64), image.size());
  EXPECT_EQ(2.0, image.devicePixelRatio());

}

TEST(ImageUtilsTest, ScaleImageKeepsAspectRatio) {

  const QImage image = ImageUtils::ScaleImage(MakeImage(300, 100), QSize(32, 32), 2.0, false);

  EXPECT_EQ(QSize(64, 21), image.size());
  EXPECT_EQ(2.0, image.devicePixelRatio());

}

TEST(ImageUtilsTest, ScaleImagePadsToKeepAspectRatio) {

  const QImage image = ImageUtils::ScaleImage(MakeImage(300, 100), QSize(32, 32), 2.0, true);

  ASSERT_EQ(QSize(64, 64), image.size());
  EXPECT_EQ(2.0, image.devicePixelRatio());
  EXPECT_EQ(0, image.pixelColor(32, 0).alpha());
  EXPECT_EQ(QColor(Qt::red), image.pixelColor(32, 32));

}

}  // namespace
