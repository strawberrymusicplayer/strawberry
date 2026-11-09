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

#include <QColor>
#include <QImage>
#include <QPalette>
#include <QTest>

#include "analyzer/boomanalyzer.h"
#include "analyzer/turbineanalyzer.h"

namespace {

// Sets the heights of the bars, and returns the canvas, which the analyzer shows while paused.
template <typename Analyzer>
class TestAnalyzer : public Analyzer {
 public:
  TestAnalyzer() : Analyzer(nullptr) {}

  void SetFirstBar(const double bar_height, const double peak_height) {
    Analyzer::bar_height_[0] = bar_height;
    Analyzer::peak_height_[0] = peak_height;
  }

  QImage Canvas() const { return Analyzer::canvas_.toImage(); }
};

// Returns true when the image has a pixel which isn't the color.
bool HasOtherColor(const QImage &image, const QColor &color) {

  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).rgb() != color.rgb()) return true;
    }
  }

  return false;

}

// Regression test: when the palette changed, the canvas was only filled with the background color, so while paused, the analyzer showed no bars until playback continued.
template <typename Analyzer>
void CheckBarsKeptWhenPaletteChanges() {

  TestAnalyzer<Analyzer> analyzer;
  analyzer.resize(200, 80);
  analyzer.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&analyzer));

  analyzer.SetFirstBar(20, 30);

  QPalette palette = analyzer.palette();
  palette.setColor(QPalette::Window, QColor(0x20, 0x20, 0x40));
  palette.setColor(QPalette::Highlight, Qt::red);
  analyzer.setPalette(palette);

  const QImage canvas = analyzer.Canvas();
  ASSERT_FALSE(canvas.isNull());
  EXPECT_TRUE(HasOtherColor(canvas, palette.color(QPalette::Window)));
  // The background has the color of the new palette.
  EXPECT_EQ(palette.color(QPalette::Window).rgb(), canvas.pixelColor(canvas.width() - 1, 0).rgb());

}

}  // namespace

TEST(BoomAnalyzerTest, BarsKeptWhenPaletteChanges) {
  CheckBarsKeptWhenPaletteChanges<BoomAnalyzer>();
}

TEST(TurbineAnalyzerTest, BarsKeptWhenPaletteChanges) {
  CheckBarsKeptWhenPaletteChanges<TurbineAnalyzer>();
}
