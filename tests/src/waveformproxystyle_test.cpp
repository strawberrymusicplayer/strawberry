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
#include <QRect>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QStandardPaths>

#include "test_utils.h"
#include "core/settings.h"
#include "constants/waveformsettings.h"

#include "waveform/waveformbuilder.h"
#include "waveform/waveformproxystyle.h"

namespace {

// Builds a well-formed SWVF blob mirroring the layout WaveformBuilder writes so
// the proxy style's renderer accepts it.
QByteArray MakeTestBlob(const int count = 100, const qint8 mn = -64, const qint8 mx = 64, const float peak = 127.0f) {

  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  stream.writeRawData(kWaveformMagic, 4);
  stream << kWaveformVersion;
  stream << static_cast<quint32>(count);
  stream << peak;
  for (int i = 0; i < count; ++i) {
    stream << mn << mx;
  }

  return data;
}

}  // namespace

TEST(WaveformProxyStyleTest, DefaultsToNoDataAndNotVisible) {

  QSlider slider;
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  // With no data and show_ off, the slider keeps the standard fixed height.
  EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

  // Delete the style (which owns the fade timeline) while the slider is still
  // alive, so no orphaned QTimer fires into a destroyed slider later.
  delete style;

}

TEST(WaveformProxyStyleTest, SetWaveformDataEmptyReverts) {

  QSlider slider;
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  // Even with show_ on, empty data must fall back to a plain (Fixed) slider.
  style->SetShowWaveform(true);
  style->SetWaveformData(QByteArray());

  EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

  delete style;

}

TEST(WaveformProxyStyleTest, SetWaveformDataAndEnableExpandsSlider) {

  QSlider slider;
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  style->SetWaveformData(MakeTestBlob());
  style->SetShowWaveform(true);

  // Both data present and show_ on: the seekbar expands vertically (REN-03).
  EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::MinimumExpanding);

  delete style;

}

TEST(WaveformProxyStyleTest, SetShowWaveformFalseCollapsesSlider) {

  QSlider slider;
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  style->SetWaveformData(MakeTestBlob());
  style->SetShowWaveform(true);
  ASSERT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::MinimumExpanding);

  style->SetShowWaveform(false);
  EXPECT_EQ(slider.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

  delete style;

}

TEST(WaveformProxyStyleTest, SubControlRectGrooveAdjustedWhenOn) {

  QSlider slider;
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  style->SetWaveformData(MakeTestBlob());
  style->SetShowWaveform(true);

  QStyleOptionSlider opt;
  opt.initFrom(&slider);
  opt.rect = QRect(0, 0, 300, 24);

  const QRect groove = style->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, &slider);

  // The groove must be inset by the public margin symbol on all sides when the
  // waveform is active. Reference the symbol, not the literal.
  EXPECT_EQ(groove, opt.rect.adjusted(WaveformProxyStyle::kWaveformMarginSize, WaveformProxyStyle::kWaveformMarginSize, -WaveformProxyStyle::kWaveformMarginSize, -WaveformProxyStyle::kWaveformMarginSize));

  delete style;

}

TEST(WaveformProxyStyleTest, PlayedUnplayedBoundaryTracksSliderValue) {

  QSlider slider;
  slider.resize(200, 40);
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  // Strong amplitude so the bars are tall enough to sample at the center line.
  style->SetWaveformData(MakeTestBlob(200, -80, 80, 127.0f));
  style->SetShowWaveform(true);

  const QSize size(200, 40);

  // SetShowWaveform starts a 1s fade (state FadingToOn). The live played/unplayed
  // split is only composited once the fade settles into WaveformOn, so drive the
  // event loop until the fade timeline finishes, then paint.
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 1500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  // Paint the styled slider's complex control to an image at a given value.
  auto paint_at = [&](const int value) {
    QStyleOptionSlider opt;
    opt.initFrom(&slider);
    opt.rect = QRect(0, 0, size.width(), size.height());
    opt.minimum = 0;
    opt.maximum = 200;
    opt.sliderValue = value;

    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawComplexControl(QStyle::CC_Slider, &opt, &painter, &slider);
    painter.end();
    return image;
  };

  const QImage at_min = paint_at(0);
  const QImage at_max = paint_at(200);

  // A pixel just left of the horizontal middle is in the unplayed region at
  // value==min but in the played region at value==max, so the split moved.
  const int x = size.width() / 2 - 10;
  const int y = size.height() / 2;
  EXPECT_NE(at_min.pixelColor(x, y), at_max.pixelColor(x, y));

  delete style;

}

TEST(WaveformProxyStyleTest, OverRangeSliderValueClampsSplit) {

  QSlider slider;
  slider.resize(200, 40);
  WaveformProxyStyle *style = new WaveformProxyStyle(&slider);

  style->SetWaveformData(MakeTestBlob(200, -80, 80, 127.0f));
  style->SetShowWaveform(true);

  const QSize size(200, 40);

  // Let the fade settle into the live WaveformOn composite.
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 1500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  auto paint_at = [&](const int value, const int maximum) {
    QStyleOptionSlider opt;
    opt.initFrom(&slider);
    opt.rect = QRect(0, 0, size.width(), size.height());
    opt.minimum = 0;
    opt.maximum = maximum;
    opt.sliderValue = value;

    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawComplexControl(QStyle::CC_Slider, &opt, &painter, &slider);
    painter.end();
    return image;
  };

  // sliderValue > maximum (TrackSlider sets max then value) must clamp x_split to
  // the groove width: the result equals painting fully played (value == maximum),
  // not an undefined negative-width clip rect. This also exercises the cursor
  // clamp so it never paints outside the groove.
  const QImage over_range = paint_at(250, 200);
  const QImage at_max = paint_at(200, 200);

  EXPECT_EQ(over_range, at_max);

  // subControlRect's handle must also clamp: the cursor rect stays inside the groove.
  QStyleOptionSlider handle_opt;
  handle_opt.initFrom(&slider);
  handle_opt.rect = QRect(0, 0, size.width(), size.height());
  handle_opt.minimum = 0;
  handle_opt.maximum = 200;
  handle_opt.sliderValue = 250;
  const QRect handle = style->subControlRect(QStyle::CC_Slider, &handle_opt, QStyle::SC_SliderHandle, &slider);
  EXPECT_LE(handle.left(), size.width());

  delete style;

}

TEST(WaveformProxyStyleTest, CustomColorChangesRenderedPixmap) {

  QStandardPaths::setTestModeEnabled(true);

  QSlider slider;
  slider.resize(200, 40);

  // Render with default (no kColor in settings — theme Highlight fallback).
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.remove(WaveformSettings::kColor);
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  WaveformProxyStyle *style_default = new WaveformProxyStyle(&slider);
  style_default->SetWaveformData(MakeTestBlob(200, -80, 80, 127.0f));
  style_default->SetShowWaveform(true);

  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 1500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  auto paint_image = [&](WaveformProxyStyle *s_ptr) {
    const QSize size(200, 40);
    QStyleOptionSlider opt;
    opt.initFrom(&slider);
    opt.rect = QRect(0, 0, size.width(), size.height());
    opt.minimum = 0;
    opt.maximum = 200;
    opt.sliderValue = 0;

    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    s_ptr->drawComplexControl(QStyle::CC_Slider, &opt, &painter, &slider);
    painter.end();
    return image;
  };

  const QImage default_image = paint_image(style_default);
  EXPECT_FALSE(default_image.isNull());

  delete style_default;

  // Now render with a distinctive custom color (pure red — very unlikely to match
  // the theme Highlight color used in the default render).
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kColor, QColor(Qt::red));
    s.setValue(WaveformSettings::kEnabled, true);
    s.endGroup();
  }

  WaveformProxyStyle *style_custom = new WaveformProxyStyle(&slider);
  style_custom->SetWaveformData(MakeTestBlob(200, -80, 80, 127.0f));
  style_custom->SetShowWaveform(true);

  timer.restart();
  while (timer.elapsed() < 1500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  const QImage custom_image = paint_image(style_custom);
  EXPECT_FALSE(custom_image.isNull());

  // The rendered images must differ: the custom red color vs the theme Highlight.
  EXPECT_NE(default_image, custom_image);

  delete style_custom;

  // Cleanup settings.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.remove(WaveformSettings::kColor);
    s.setValue(WaveformSettings::kEnabled, false);
    s.endGroup();
  }

}
