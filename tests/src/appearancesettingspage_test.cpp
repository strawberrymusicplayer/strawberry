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

#include <memory>

#include "gtest_include.h"

#include <QApplication>
#include <QString>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QPushButton>
#include <QRadioButton>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/appearance.h"
#include "settings/appearancesettingspage.h"
#include "constants/appearancesettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {

class AppearanceSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings().remove(QLatin1String(AppearanceSettings::kSettingsGroup));
    // Native styles on Windows and macOS, and Breeze, don't support custom palette colors, then the custom colors are not saved, so the tests use Fusion with its own palette, which doesn't depend on the platform.
    original_style_ = QApplication::style()->name();
    ASSERT_NE(nullptr, QApplication::setStyle(u"Fusion"_s));
    QApplication::setPalette(QApplication::style()->standardPalette());
    appearance_ = make_shared<Appearance>();
    // The page uses the default style for the "default" entry in the style list.
    appearance_->set_default_style(QApplication::style()->objectName());
    system_window_color_ = QApplication::palette().color(QPalette::Window);
    dark_window_color_ = Appearance::DarkColors().value(QPalette::Window);
  }

  void TearDown() override {
    Settings().remove(QLatin1String(AppearanceSettings::kSettingsGroup));
    if (!original_style_.isEmpty()) {
      QApplication::setStyle(original_style_);
    }
    QApplication::setPalette(QPalette());
  }

  // Switches to the custom color set with the dark colors, like clicking the button for dark colors.
  static void SelectDarkColors(AppearanceSettingsPage *page) {
    QPushButton *button_dark_colors = page->findChild<QPushButton*>(u"button_dark_colors"_s);
    ASSERT_NE(nullptr, button_dark_colors);
    Q_EMIT button_dark_colors->pressed();
  }

  static bool UseCustomColorSetChecked(AppearanceSettingsPage *page) {
    QRadioButton *radiobutton_use_custom_color_set = page->findChild<QRadioButton*>(u"use_custom_color_set"_s);
    return radiobutton_use_custom_color_set && radiobutton_use_custom_color_set->isChecked();
  }

  QString original_style_;
  SharedPtr<Appearance> appearance_;
  QColor system_window_color_;
  QColor dark_window_color_;
};

// Cancel without Apply restores the look from when the dialog was opened.
TEST_F(AppearanceSettingsPageTest, CancelRestoresColors) {

  ASSERT_NE(system_window_color_, dark_window_color_);

  AppearanceSettingsPage page(nullptr, appearance_);
  ASSERT_FALSE(UseCustomColorSetChecked(&page));

  SelectDarkColors(&page);
  if (HasFatalFailure()) return;
  ASSERT_EQ(dark_window_color_, QApplication::palette().color(QPalette::Window));

  page.Reject();
  EXPECT_EQ(system_window_color_, QApplication::palette().color(QPalette::Window));

  Settings s;
  s.beginGroup(QLatin1String(AppearanceSettings::kSettingsGroup));
  EXPECT_FALSE(s.value(QLatin1String(AppearanceSettings::kUseCustomColorSet), false).toBool());
  s.endGroup();

}

// Regression test: Cancel after Apply restored the look from when the dialog was opened, while the applied settings were kept, so the look didn't match the settings until the next restart.
TEST_F(AppearanceSettingsPageTest, CancelAfterApplyKeepsAppliedColors) {

  ASSERT_NE(system_window_color_, dark_window_color_);

  AppearanceSettingsPage page(nullptr, appearance_);
  ASSERT_FALSE(UseCustomColorSetChecked(&page));

  SelectDarkColors(&page);
  if (HasFatalFailure()) return;
  page.Apply();

  Settings s;
  s.beginGroup(QLatin1String(AppearanceSettings::kSettingsGroup));
  ASSERT_TRUE(s.value(QLatin1String(AppearanceSettings::kUseCustomColorSet), false).toBool());
  s.endGroup();

  page.Reject();
  EXPECT_EQ(dark_window_color_, QApplication::palette().color(QPalette::Window));

}

}  // namespace
