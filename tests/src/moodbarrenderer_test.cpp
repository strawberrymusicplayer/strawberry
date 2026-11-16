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

#include "config.h"

#include "gtest_include.h"

#include <QImage>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QSize>

#include "moodbar/moodbarrenderer.h"

namespace {

const QColor kColor = QColor::fromHsv(120, 200, 150);

// Regression test: with a height of one row, the middle row was drawn with a division by zero.
TEST(MoodbarRendererTest, OneRowHasTheFullColor) {

  const QImage image = MoodbarRenderer::RenderToImage(ColorVector(10, kColor), QSize(10, 1));
  const QColor color = image.pixelColor(5, 0);
  EXPECT_EQ(kColor.hue(), color.hue());
  EXPECT_NEAR(kColor.saturation(), color.saturation(), 2);
  EXPECT_NEAR(kColor.value(), color.value(), 2);

}

TEST(MoodbarRendererTest, MiddleRowHasTheFullColorAndEdgesAreLighter) {

  for (const int height : {4, 5}) {
    const QImage image = MoodbarRenderer::RenderToImage(ColorVector(10, kColor), QSize(10, height));
    const QColor middle = image.pixelColor(5, height / 2);
    EXPECT_EQ(kColor.hue(), middle.hue()) << "height " << height;
    EXPECT_NEAR(kColor.saturation(), middle.saturation(), 2) << "height " << height;
    EXPECT_NEAR(kColor.value(), middle.value(), 2) << "height " << height;
    const QColor top = image.pixelColor(5, 0);
    const QColor bottom = image.pixelColor(5, height - 1);
    EXPECT_LT(top.saturation(), middle.saturation()) << "height " << height;
    EXPECT_GT(top.value(), middle.value()) << "height " << height;
    EXPECT_EQ(top, bottom) << "height " << height;
  }

}

TEST(MoodbarRendererTest, EmptyRectIsNotDrawn) {

  QImage image(10, 3, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  {
    QPainter p(&image);
    MoodbarRenderer::Render(ColorVector(10, kColor), &p, QRect(0, 1, 10, 0));
  }
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      EXPECT_EQ(0, qAlpha(image.pixel(x, y))) << x << "," << y;
    }
  }

}

}  // namespace
