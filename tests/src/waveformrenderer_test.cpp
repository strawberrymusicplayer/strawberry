/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include <QtGlobal>
#include <QByteArray>
#include <QIODevice>
#include <QDataStream>
#include <QSize>
#include <QPalette>
#include <QPixmap>
#include <QImage>
#include <QColor>

#include "test_utils.h"

#include "waveform/waveformbuilder.h"
#include "waveform/waveformrenderer.h"

namespace {

// Builds a well-formed, fixed-resolution SWVF blob (WaveformBuilder::kWaveformBaseCount buckets), each holding mn/mx, and a caller-supplied header peak.
// The format is fixed-resolution, so the body is always WaveformBuilder::kWaveformBaseCount pairs regardless of the count argument (kept for call-site readability) — this is what WaveformBuilder::IsValidBlob now requires.
QByteArray MakeTestBlob(const int count = WaveformBuilder::kWaveformBaseCount, const qint8 mn = -64, const qint8 mx = 64, const float peak = 127.0F) {

  Q_UNUSED(count)

  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  stream.writeRawData(WaveformBuilder::kWaveformMagic, 4);
  stream << WaveformBuilder::kWaveformVersion;
  stream << static_cast<quint32>(WaveformBuilder::kWaveformBaseCount);
  stream << peak;
  for (int i = 0; i < WaveformBuilder::kWaveformBaseCount; ++i) {
    stream << mn << mx;
  }

  return data;
}

}  // namespace

TEST(WaveformRendererTest, ReturnsNullPixmapForZeroSize) {

  const QByteArray data = MakeTestBlob();
  const QPalette palette;
  const QPixmap pixmap = WaveformRenderer::RenderToPixmap(data, QSize(0, 0), palette, QColor(Qt::blue));
  EXPECT_TRUE(pixmap.isNull());

}

TEST(WaveformRendererTest, ReturnsNullPixmapForInvalidBlob) {

  // Garbage bytes do not satisfy the SWVF magic/version/length checks.
  const QByteArray garbage("not a valid waveform blob");
  const QPalette palette;
  const QPixmap pixmap = WaveformRenderer::RenderToPixmap(garbage, QSize(100, 40), palette, QColor(Qt::blue));
  EXPECT_TRUE(pixmap.isNull());

}

TEST(WaveformRendererTest, RendersNonNullForValidBlob) {

  const QByteArray data = MakeTestBlob();
  const QPalette palette;
  const QPixmap pixmap = WaveformRenderer::RenderToPixmap(data, QSize(100, 40), palette, QColor(Qt::blue));
  ASSERT_FALSE(pixmap.isNull());
  EXPECT_EQ(pixmap.size(), QSize(100, 40));

}

TEST(WaveformRendererTest, SilentTrackPeakZeroDoesNotCrash) {

  // A structurally valid blob with a zero header peak reaches the renderer's peak parse; it must not divide by zero (peak is consumed, never used to normalize).
  // A flat zero envelope simply produces no bars.
  const QByteArray data = MakeTestBlob(100, 0, 0, 0.0F);
  ASSERT_TRUE(WaveformBuilder::IsValidBlob(data));
  const QPalette palette;
  const QPixmap pixmap = WaveformRenderer::RenderToPixmap(data, QSize(100, 40), palette, QColor(Qt::blue));
  EXPECT_FALSE(pixmap.isNull());

}

TEST(WaveformRendererTest, DifferentBarColorsProduceDifferentPixmaps) {

  // The bar color is caller-supplied; rendering the same blob in two colors must yield two visibly different images (the proxy relies on this to composite a played pixmap and an unplayed pixmap around the live split).
  const QByteArray data = MakeTestBlob(200, -80, 80, 127.0F);
  const QPalette palette;

  const QPixmap blue = WaveformRenderer::RenderToPixmap(data, QSize(200, 40), palette, QColor(Qt::blue));
  const QPixmap red = WaveformRenderer::RenderToPixmap(data, QSize(200, 40), palette, QColor(Qt::red));
  ASSERT_FALSE(blue.isNull());
  ASSERT_FALSE(red.isNull());

  EXPECT_NE(blue.toImage(), red.toImage());

}

TEST(WaveformRendererTest, GentleCurvePreservesLoudQuietContrast) {

  // The 0.65 power-law must keep a loud track's bars taller than a quiet track's (it lifts quiet passages without saturating everything to full height).
  const QByteArray loud = MakeTestBlob(100, -120, 120, 127.0F);
  const QByteArray quiet = MakeTestBlob(100, -16, 16, 127.0F);
  const QPalette palette;

  const QPixmap loud_pixmap = WaveformRenderer::RenderToPixmap(loud, QSize(100, 40), palette, QColor(Qt::blue));
  const QPixmap quiet_pixmap = WaveformRenderer::RenderToPixmap(quiet, QSize(100, 40), palette, QColor(Qt::blue));
  ASSERT_FALSE(loud_pixmap.isNull());
  ASSERT_FALSE(quiet_pixmap.isNull());

  const QImage loud_image = loud_pixmap.toImage();
  const QImage quiet_image = quiet_pixmap.toImage();
  const QColor bg = palette.color(QPalette::Active, QPalette::Window);

  // Count painted (non-background) pixels in a column; a louder track paints more of the column height than a quieter one.
  auto painted_height = [&](const QImage &image) {
    int painted = 0;
    for (int y = 0; y < image.height(); ++y) {
      if (image.pixelColor(50, y) != bg) ++painted;
    }
    return painted;
  };

  EXPECT_GT(painted_height(loud_image), painted_height(quiet_image));

}
