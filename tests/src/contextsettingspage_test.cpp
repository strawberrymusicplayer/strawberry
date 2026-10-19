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

#include <QString>
#include <QVariant>
#include <QCheckBox>

#include "core/settings.h"
#include "constants/contextsettings.h"
#include "settings/contextsettingspage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class ContextSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings s;
    s.remove(QLatin1String(ContextSettings::kSettingsGroup));
  }

  void TearDown() override {
    Settings s;
    s.remove(QLatin1String(ContextSettings::kSettingsGroup));
  }

  static QVariant SavedClickToSeek() {
    Settings s;
    s.beginGroup(ContextSettings::kSettingsGroup);
    return s.value(ContextSettings::kSyncedLyricsClickToSeek);
  }

  static QCheckBox *CheckBox(ContextSettingsPage *page, const QString &name) { return page->findChild<QCheckBox*>(name); }
};

// Seeking by clicking a synchronized lyrics line is off by default, and saved when turned on.
TEST_F(ContextSettingsPageTest, ClickToSeekIsOffByDefaultAndSaved) {

  {
    ContextSettingsPage page(nullptr);
    page.Load();
    QCheckBox *checkbox_click_to_seek = CheckBox(&page, u"checkbox_synced_lyrics_click_to_seek"_s);
    ASSERT_NE(nullptr, checkbox_click_to_seek);
    EXPECT_FALSE(checkbox_click_to_seek->isChecked());
    EXPECT_EQ(ContextSettings::kDefaultSyncedLyricsClickToSeek, checkbox_click_to_seek->isChecked());
    checkbox_click_to_seek->setChecked(true);
    page.Save();
  }

  EXPECT_TRUE(SavedClickToSeek().toBool());

  ContextSettingsPage page(nullptr);
  page.Load();
  EXPECT_TRUE(CheckBox(&page, u"checkbox_synced_lyrics_click_to_seek"_s)->isChecked());

}

// The option only applies when synchronized lyrics are shown.
TEST_F(ContextSettingsPageTest, ClickToSeekIsDisabledWithoutSyncedLyrics) {

  ContextSettingsPage page(nullptr);
  page.Load();
  QCheckBox *checkbox_song_lyrics = CheckBox(&page, u"checkbox_song_lyrics"_s);
  QCheckBox *checkbox_synced_lyrics = CheckBox(&page, u"checkbox_synced_lyrics"_s);
  QCheckBox *checkbox_click_to_seek = CheckBox(&page, u"checkbox_synced_lyrics_click_to_seek"_s);
  ASSERT_TRUE(checkbox_song_lyrics->isChecked());
  ASSERT_TRUE(checkbox_synced_lyrics->isChecked());
  EXPECT_TRUE(checkbox_click_to_seek->isEnabled());

  checkbox_synced_lyrics->setChecked(false);
  EXPECT_FALSE(checkbox_click_to_seek->isEnabled());

  checkbox_synced_lyrics->setChecked(true);
  checkbox_song_lyrics->setChecked(false);
  EXPECT_FALSE(checkbox_click_to_seek->isEnabled());

}

}  // namespace
