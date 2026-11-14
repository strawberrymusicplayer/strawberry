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

#include <memory>

#include <QApplication>
#include <QDialog>
#include <QString>
#include <QTimer>
#include <QGroupBox>
#include <QRadioButton>
#include <QTreeWidget>

#include "core/settings.h"
#include "constants/globalshortcutssettings.h"
#include "globalshortcuts/globalshortcutsmanager.h"
#include "settings/globalshortcutssettingspage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class GlobalShortcutsSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
    // The native backend is always used on macOS and Windows, it registers the shortcuts with the system.
    GTEST_SKIP() << "The native global shortcuts backend can't be disabled on this platform.";
#endif

    {
      Settings s;
      s.remove(QLatin1String(GlobalShortcutsSettings::kSettingsGroup));
      s.beginGroup(GlobalShortcutsSettings::kSettingsGroup);
      // The shortcuts are not registered with the desktop by the test.
      s.setValue(GlobalShortcutsSettings::kUseKGlobalAccel, false);
      s.setValue(GlobalShortcutsSettings::kUseX11, false);
      s.endGroup();
    }

    manager_ = std::make_unique<GlobalShortcutsManager>();
    page_ = std::make_unique<GlobalShortcutsSettingsPage>(nullptr, &*manager_);
    page_->Load();
    // The options are only enabled with a backend, which the test doesn't use.
    page_->findChild<QGroupBox*>(u"shortcut_options"_s)->setEnabled(true);

    // The first shortcut in the list has a default key.
    QTreeWidget *list = page_->findChild<QTreeWidget*>(u"list"_s);
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
      if (!manager_->shortcuts().value(list->topLevelItem(i)->data(0, Qt::UserRole).toString()).default_key.isEmpty()) {
        list->setCurrentItem(list->topLevelItem(i));
        break;
      }
    }

  }

  void TearDown() override {
    page_.reset();
    manager_.reset();
    Settings s;
    s.remove(QLatin1String(GlobalShortcutsSettings::kSettingsGroup));
  }

  QRadioButton *Radio(const QString &name) const { return page_->findChild<QRadioButton*>(name); }

  std::unique_ptr<GlobalShortcutsManager> manager_;
  std::unique_ptr<GlobalShortcutsSettingsPage> page_;
};

// Regression test: clicking the custom radio button and cancelling the key grabber left the custom radio button checked, while the shortcut was still the default one.
TEST_F(GlobalShortcutsSettingsPageTest, CancelledGrabberRestoresRadioButton) {

  ASSERT_TRUE(Radio(u"radio_default"_s)->isChecked());

  QTimer::singleShot(0, []() {
    QDialog *grabber = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    ASSERT_NE(nullptr, grabber);
    grabber->reject();
  });
  Radio(u"radio_custom"_s)->click();

  EXPECT_TRUE(Radio(u"radio_default"_s)->isChecked());
  EXPECT_FALSE(Radio(u"radio_custom"_s)->isChecked());

}

// Regression test: when the settings dialog was cancelled and opened again, the shortcuts were restored, but the radio buttons still showed the cancelled shortcut.
TEST_F(GlobalShortcutsSettingsPageTest, LoadRestoresRadioButton) {

  Radio(u"radio_none"_s)->click();
  ASSERT_TRUE(Radio(u"radio_none"_s)->isChecked());

  page_->Load();

  EXPECT_TRUE(Radio(u"radio_default"_s)->isChecked());
  EXPECT_FALSE(Radio(u"radio_none"_s)->isChecked());

}

}  // namespace
