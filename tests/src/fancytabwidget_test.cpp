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
#include <QList>
#include <QPoint>
#include <QRect>
#include <QRgb>
#include <QStackedWidget>
#include <QTabBar>
#include <QTest>

#include "widgets/fancytabwidget.h"
#include "widgets/fancytabbar.h"
#include "core/settings.h"

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

// FancyTabWidget wraps each content widget in its own page, so widget(), currentWidget() and setCurrentWidget() never match the content widget passed to AddTab().
// Code that looks up a tab by its content widget must use IndexOfTab().
TEST(FancyTabWidgetTest, ContentWidgetIsWrappedInPage) {

  FancyTabWidget tabs;
  QWidget *first_view = new QWidget;
  QWidget *second_view = new QWidget;
  tabs.AddTab(first_view, u"first"_s, QIcon(), u"First"_s);
  tabs.AddTab(second_view, u"second"_s, QIcon(), u"Second"_s);
  ASSERT_TRUE(tabs.EnableTab(first_view));
  ASSERT_TRUE(tabs.EnableTab(second_view));

  const int second_index = tabs.IndexOfTab(second_view);
  ASSERT_GE(second_index, 0);
  EXPECT_NE(tabs.widget(second_index), second_view);
  EXPECT_TRUE(tabs.widget(second_index)->isAncestorOf(second_view));
  EXPECT_EQ(tabs.indexOf(second_view), -1);

  tabs.setCurrentIndex(second_index);
  EXPECT_EQ(tabs.currentIndex(), tabs.IndexOfTab(second_view));
  EXPECT_NE(tabs.currentWidget(), second_view);

}

// Regression test: on Android, tapping the already active Playlists tab must return from the playlist editor to the playlist list.
// The main window compared the wrapper page with the content widget, so the comparison always failed and the editor stayed open.
TEST(FancyTabWidgetTest, ClickingActiveTabFindsContentWidget) {

  FancyTabWidget tabs;
  QWidget *other_view = new QWidget;
  QStackedWidget *pages = new QStackedWidget;
  QWidget *list_page = new QWidget;
  QWidget *editor_page = new QWidget;
  pages->addWidget(list_page);
  pages->addWidget(editor_page);
  tabs.AddTab(other_view, u"other"_s, QIcon(), u"Other"_s);
  tabs.AddTab(pages, u"playlists"_s, QIcon(), u"Playlists"_s);
  ASSERT_TRUE(tabs.EnableTab(other_view));
  ASSERT_TRUE(tabs.EnableTab(pages));
  tabs.SetMode(FancyTabWidget::Mode::LargeSidebar);

  // Same lookups as MainWindow::SetupAndroidLayout().
  QObject::connect(tabs.tabBar(), &QTabBar::tabBarClicked, &tabs, [&tabs, pages, list_page](const int index) {
    if (index == tabs.IndexOfTab(pages)) pages->setCurrentWidget(list_page);
  });
  QObject::connect(&tabs, &QTabWidget::currentChanged, &tabs, [&tabs, pages, list_page]() {
    if (tabs.currentIndex() == tabs.IndexOfTab(pages)) pages->setCurrentWidget(list_page);
  });

  tabs.resize(600, 600);
  tabs.show();
  QCoreApplication::processEvents();

  const int pages_index = tabs.IndexOfTab(pages);
  ASSERT_GE(pages_index, 0);
  tabs.setCurrentIndex(pages_index);
  pages->setCurrentWidget(editor_page);

  // Click the tab which is already active.
  QTest::mouseClick(tabs.tabBar(), Qt::LeftButton, Qt::NoModifier, tabs.tabBar()->tabRect(pages_index).center());
  EXPECT_EQ(tabs.currentIndex(), pages_index);
  EXPECT_EQ(pages->currentWidget(), list_page);

  // Switching to another tab and back also shows the list.
  pages->setCurrentWidget(editor_page);
  tabs.setCurrentIndex(tabs.IndexOfTab(other_view));
  tabs.setCurrentIndex(pages_index);
  EXPECT_EQ(pages->currentWidget(), list_page);

}

// Regression test: a newly added tab without a saved position can request the same index as a tab with a saved position.
// The tabs are stored in a hash, so the collision was resolved in an arbitrary order, and the new Android Tools tab sometimes ended up after Settings.
TEST(FancyTabWidgetTest, LoadSettingsResolvesCollisionsInAddOrder) {

  const QString settings_group = u"FancyTabWidgetTest"_s;
  {
    Settings s;
    s.beginGroup(settings_group);
    s.setValue(u"tab_settings"_s, 8);
    s.endGroup();
  }

  // The hash order depends on the widget addresses, so repeat with new widgets to catch an arbitrary order.
  for (int attempt = 0; attempt < 20; ++attempt) {
    FancyTabWidget tabs;
    for (int i = 0; i < 8; ++i) {
      tabs.AddTab(new QWidget, u"tab%1"_s.arg(i), QIcon(), u"Tab %1"_s.arg(i));
    }
    QWidget *tools_view = new QWidget;
    QWidget *settings_view = new QWidget;
    tabs.AddTab(tools_view, u"tools"_s, QIcon(), u"Tools"_s);
    tabs.AddTab(settings_view, u"settings"_s, QIcon(), u"Settings"_s);
    tabs.LoadSettings(settings_group);

    EXPECT_EQ(tabs.IndexOfTab(tools_view), 8);
    EXPECT_EQ(tabs.IndexOfTab(settings_view), 9);
  }

  Settings s;
  s.remove(settings_group);

}

namespace {

// A sidebar with more tabs than fit, set up like on Android.
class TouchScrollingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (int i = 0; i < 10; ++i) {
      QWidget *view = new QWidget;
      tabs_.AddTab(view, u"tab%1"_s.arg(i), QIcon(), u"Tab %1"_s.arg(i));
      ASSERT_TRUE(tabs_.EnableTab(view));
      views_ << view;
    }
    tabs_.SetMode(FancyTabWidget::Mode::LargeSidebar);
    tabs_.setMovable(false);
    tab_bar_ = qobject_cast<FancyTabBar*>(tabs_.tabBar());
    ASSERT_TRUE(tab_bar_);
    tab_bar_->SetTouchScrolling(true);
    tabs_.resize(600, 250);
    tabs_.show();
    QCoreApplication::processEvents();
    tabs_.setCurrentIndex(0);
  }

  FancyTabWidget tabs_;
  FancyTabBar *tab_bar_ = nullptr;
  QList<QWidget*> views_;
};

}  // namespace

// Regression test: on Android, dragging the sidebar moved the tabs to another position instead of scrolling the tabs that don't fit.
TEST_F(TouchScrollingTest, DragScrollsWithoutSelectingOrMovingTabs) {

  ASSERT_GT(tab_bar_->sizeHint().height(), tab_bar_->height());

  int clicks = 0;
  QObject::connect(tab_bar_, &QTabBar::tabBarClicked, tab_bar_, [&clicks]() { ++clicks; });

  const QRect first_tab_rect = tab_bar_->tabRect(0);
  QPoint position = tab_bar_->tabRect(1).center();
  QTest::mousePress(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);
  for (int i = 0; i < 5; ++i) {
    position.ry() -= 20;
    QTest::mouseMove(tab_bar_, position);
  }
  QTest::mouseRelease(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);

  EXPECT_LT(tab_bar_->tabRect(0).top(), first_tab_rect.top());
  EXPECT_EQ(tabs_.currentIndex(), 0);
  EXPECT_EQ(clicks, 0);
  for (qsizetype i = 0; i < views_.count(); ++i) {
    EXPECT_EQ(tabs_.IndexOfTab(views_[i]), i);
  }

  // Dragging back down scrolls back to the first tab.
  QTest::mousePress(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);
  for (int i = 0; i < 10; ++i) {
    position.ry() += 20;
    QTest::mouseMove(tab_bar_, position);
  }
  QTest::mouseRelease(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);
  EXPECT_EQ(tab_bar_->tabRect(0).top(), first_tab_rect.top());
  EXPECT_EQ(tabs_.currentIndex(), 0);

}

TEST_F(TouchScrollingTest, TapSelectsTab) {

  int clicked_index = -1;
  QObject::connect(tab_bar_, &QTabBar::tabBarClicked, tab_bar_, [&clicked_index](const int index) { clicked_index = index; });

  const QPoint position = tab_bar_->tabRect(1).center();
  QTest::mousePress(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);
  EXPECT_EQ(tabs_.currentIndex(), 0);
  QTest::mouseRelease(tab_bar_, Qt::LeftButton, Qt::NoModifier, position);

  EXPECT_EQ(clicked_index, 1);
  EXPECT_EQ(tabs_.currentIndex(), 1);

}
