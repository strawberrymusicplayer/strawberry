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

#include <QCoreApplication>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPixmapCache>
#include <QSet>
#include <QRgb>

#include "widgets/fancytabwidget.h"
#include "widgets/fancytabbar.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Returns the number of different colors in a column of the image, between the rows.
qsizetype ColorsInColumn(const QImage &image, const int x, const int top, const int bottom) {

  QSet<QRgb> colors;
  for (int y = top; y <= bottom; ++y) {
    colors.insert(image.pixel(x, y));
  }

  return colors.count();

}

}  // namespace

// Regression test: the background graphics of the sidebar was only drawn in its own width, so the sidebar with labels, which is wider, had no background graphics on the right side.
TEST(FancyTabWidgetTest, BackgroundPixmapFillsWideSidebar) {

  QPixmapCache::clear();

  const QPixmap background_pixmap(u":/pictures/sidebar-background.png"_s);
  ASSERT_FALSE(background_pixmap.isNull());

  FancyTabWidget tabs;
  tabs.SetBackgroundPixmap(background_pixmap);
  // A long label makes the sidebar wider than the background graphics.
  // Add the tab like the main window does, the label is taken from the tab data, which InsertTab() with a label doesn't set.
  QWidget *view = new QWidget;
  tabs.AddTab(view, u"smartplaylists"_s, QIcon(), u"Intelligente Wiedergabelisten"_s);
  ASSERT_TRUE(tabs.EnableTab(view));
  tabs.SetMode(FancyTabWidget::Mode::LargeSidebar);
  tabs.resize(600, 600);

  // Show the widget, so the sidebar is laid out with the width of the label, otherwise it has the default width of the tab bar.
  tabs.show();
  QCoreApplication::processEvents();

  const QImage image = tabs.grab().toImage();
  const int sidebar_width = tabs.tabBar()->width();
  ASSERT_GT(sidebar_width, background_pixmap.width() + 10);

  // The image is in device pixels, the widget coordinates are in logical pixels.
  const qreal device_pixel_ratio = image.devicePixelRatio();
  const auto column = [device_pixel_ratio](const int x) { return static_cast<int>(x * device_pixel_ratio); };

  // Below the tab, only the background is drawn, the gradient is horizontal, so a column without the graphics has one color.
  const int top = image.height() / 2;
  const int bottom = image.height() - 1;
  EXPECT_GT(ColorsInColumn(image, column(background_pixmap.width() / 2), top, bottom), 1);
  EXPECT_GT(ColorsInColumn(image, column(background_pixmap.width() + 5), top, bottom), 1);
  EXPECT_GT(ColorsInColumn(image, column(sidebar_width - 5), top, bottom), 1);

}
