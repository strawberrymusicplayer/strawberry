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

#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

#include "core/settings.h"
#include "constants/playlistsettings.h"
#include "playlist/playlisttabbar.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistTabBarTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Don't ask before closing, the tab bar has no playlist manager to check the playlists.
    Settings s;
    s.beginGroup(PlaylistSettings::kSettingsGroup);
    s.setValue(PlaylistSettings::kWarnClosePlaylist, false);
    s.endGroup();

    tab_bar_.InsertTab(1, 0, u"Playlist 1"_s, false);
    tab_bar_.InsertTab(2, 1, u"Playlist 2"_s, false);
    tab_bar_.InsertTab(3, 2, u"Playlist 3"_s, false);

    // The playlist container removes the tab when the playlist manager has closed the playlist.
    QObject::connect(&tab_bar_, &PlaylistTabBar::Close, &tab_bar_, [this](const int id) { tab_bar_.RemoveTab(id); });
  }

  void TearDown() override {
    Settings().remove(QLatin1String(PlaylistSettings::kSettingsGroup));
  }

  int CurrentId() const { return tab_bar_.tabData(tab_bar_.currentIndex()).toInt(); }

  PlaylistTabBar tab_bar_;
};

// Counts the tabs laid out, every tab is measured when the tabs are laid out.
class CountingPlaylistTabBar : public PlaylistTabBar {
 public:
  int tab_size_hints() const { return tab_size_hints_; }
  void reset_tab_size_hints() { tab_size_hints_ = 0; }

 protected:
  QSize tabSizeHint(const int index) const override {
    ++tab_size_hints_;
    return PlaylistTabBar::tabSizeHint(index);
  }

 private:
  mutable int tab_size_hints_ = 0;
};

QIcon MakeIcon(const Qt::GlobalColor color) {
  QPixmap pixmap(16, 16);
  pixmap.fill(color);
  return QIcon(pixmap);
}

// Regression test: setting the icon of the active playlist set the icon of every tab, which laid out all the tabs for every tab, so playing and pausing was slow with many playlists.
TEST(PlaylistTabBarActiveIconTest, ChangingActiveIconDoesNotLayOutTabsForEveryTab) {

  constexpr int kTabs = 50;
  CountingPlaylistTabBar tab_bar;
  for (int i = 0; i < kTabs; ++i) {
    tab_bar.InsertTab(i + 1, i, u"Playlist %1"_s.arg(i + 1), false);
  }
  // The tabs are only laid out when the tab bar is visible.
  tab_bar.show();

  const QIcon play_icon = MakeIcon(Qt::green);
  const QIcon pause_icon = MakeIcon(Qt::red);

  // Laying out the tabs measures each tab twice, each change of the icons can lay out the tabs once.
  tab_bar.reset_tab_size_hints();
  tab_bar.SetActiveIcon(5, play_icon);
  EXPECT_LE(tab_bar.tab_size_hints(), 2 * kTabs);

  // Changing the icon of the tab with an icon doesn't lay out the tabs.
  tab_bar.reset_tab_size_hints();
  tab_bar.SetActiveIcon(5, pause_icon);
  EXPECT_EQ(0, tab_bar.tab_size_hints());

  tab_bar.reset_tab_size_hints();
  tab_bar.SetActiveIcon(7, play_icon);
  EXPECT_LE(tab_bar.tab_size_hints(), 4 * kTabs);

  for (int i = 0; i < kTabs; ++i) {
    EXPECT_EQ(i == tab_bar.index_of(7), !tab_bar.tabIcon(i).isNull()) << i;
  }
  EXPECT_EQ(play_icon.cacheKey(), tab_bar.tabIcon(tab_bar.index_of(7)).cacheKey());

  // Stopped.
  tab_bar.reset_tab_size_hints();
  tab_bar.SetActiveIcon(7, QIcon());
  EXPECT_LE(tab_bar.tab_size_hints(), 2 * kTabs);
  for (int i = 0; i < kTabs; ++i) {
    EXPECT_TRUE(tab_bar.tabIcon(i).isNull()) << i;
  }

}

// Regression test: closing a tab which wasn't the current tab, like with its close button, switched to the tab left of it.
TEST_F(PlaylistTabBarTest, ClosingOtherTabKeepsCurrentTab) {

  tab_bar_.setCurrentIndex(0);
  ASSERT_EQ(1, CurrentId());

  Q_EMIT tab_bar_.tabCloseRequested(2);

  ASSERT_EQ(2, tab_bar_.count());
  EXPECT_EQ(1, CurrentId());

}

// Closing the current tab selects the tab left of it.
TEST_F(PlaylistTabBarTest, ClosingCurrentTabSelectsTabLeftOfIt) {

  tab_bar_.setCurrentIndex(2);
  ASSERT_EQ(3, CurrentId());

  Q_EMIT tab_bar_.tabCloseRequested(2);

  ASSERT_EQ(2, tab_bar_.count());
  EXPECT_EQ(2, CurrentId());

}

}  // namespace
