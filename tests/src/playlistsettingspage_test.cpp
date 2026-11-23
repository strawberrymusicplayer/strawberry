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
#include <QCheckBox>

#include "core/settings.h"
#include "constants/playlistsettings.h"
#include "settings/playlistsettingspage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class PlaylistSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings s;
    s.remove(QLatin1String(PlaylistSettings::kSettingsGroup));
  }

  void TearDown() override {
    Settings s;
    s.remove(QLatin1String(PlaylistSettings::kSettingsGroup));
  }

  static void SetBarsAndGlow(const bool bars, const bool glow) {
    Settings s;
    s.beginGroup(PlaylistSettings::kSettingsGroup);
    s.setValue(PlaylistSettings::kShowBars, bars);
    s.setValue(PlaylistSettings::kGlowEffect, glow);
    s.endGroup();
  }

  static bool SavedGlow() {
    Settings s;
    s.beginGroup(PlaylistSettings::kSettingsGroup);
    return s.value(PlaylistSettings::kGlowEffect).toBool();
  }
};

// Regression test: the glow setting wasn't loaded while the bars were turned off, so saving the settings turned the glow effect off, and it was off when the bars were turned on again.
TEST_F(PlaylistSettingsPageTest, SavingWithBarsOffKeepsGlow) {

  SetBarsAndGlow(false, true);

  PlaylistSettingsPage page(nullptr);
  page.Load();
  page.Save();

  EXPECT_TRUE(SavedGlow());

}

// The glow checkbox shows the setting while the bars are turned off, and is disabled until they are turned on.
TEST_F(PlaylistSettingsPageTest, GlowCheckboxShowsSettingWithBarsOff) {

  SetBarsAndGlow(false, true);

  PlaylistSettingsPage page(nullptr);
  page.Load();

  QCheckBox *checkbox_bars = page.findChild<QCheckBox*>(u"checkbox_barscurrenttrack"_s);
  QCheckBox *checkbox_glow = page.findChild<QCheckBox*>(u"checkbox_glowcurrenttrack"_s);
  ASSERT_NE(nullptr, checkbox_bars);
  ASSERT_NE(nullptr, checkbox_glow);
  EXPECT_FALSE(checkbox_glow->isEnabled());
  EXPECT_TRUE(checkbox_glow->isChecked());

  checkbox_bars->setChecked(true);
  EXPECT_TRUE(checkbox_glow->isEnabled());
  EXPECT_TRUE(checkbox_glow->isChecked());

}

// Turning the glow effect off with the bars on is saved.
TEST_F(PlaylistSettingsPageTest, SavingWithBarsOnSavesGlow) {

  SetBarsAndGlow(true, true);

  PlaylistSettingsPage page(nullptr);
  page.Load();
  page.findChild<QCheckBox*>(u"checkbox_glowcurrenttrack"_s)->setChecked(false);
  page.Save();

  EXPECT_FALSE(SavedGlow());

}

}  // namespace
