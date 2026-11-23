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

#include <QKeySequence>
#include <QString>
#include <QAction>
#include <QList>
#include <QShortcut>

#include "core/settings.h"
#include "constants/globalshortcutssettings.h"
#include "globalshortcuts/globalshortcutsmanager.h"

using namespace Qt::Literals::StringLiterals;

namespace {

TEST(GlobalShortcutsManagerTest, NormalizeKeySequenceReplacesTogglePlayPause) {

  EXPECT_EQ(QKeySequence(Qt::Key_MediaPlay), GlobalShortcutsManager::NormalizeKeySequence(QKeySequence(Qt::Key_MediaTogglePlayPause)));
  EXPECT_EQ(QKeySequence(Qt::ControlModifier | Qt::Key_MediaPlay), GlobalShortcutsManager::NormalizeKeySequence(QKeySequence(Qt::ControlModifier | Qt::Key_MediaTogglePlayPause)));
  EXPECT_EQ(QKeySequence(Qt::Key_A, Qt::Key_MediaPlay), GlobalShortcutsManager::NormalizeKeySequence(QKeySequence(Qt::Key_A, Qt::Key_MediaTogglePlayPause)));

}

TEST(GlobalShortcutsManagerTest, NormalizeKeySequenceKeepsOtherKeys) {

  EXPECT_EQ(QKeySequence(Qt::Key_MediaNext), GlobalShortcutsManager::NormalizeKeySequence(QKeySequence(Qt::Key_MediaNext)));
  EXPECT_EQ(QKeySequence(Qt::ShiftModifier | Qt::Key_F1), GlobalShortcutsManager::NormalizeKeySequence(QKeySequence(Qt::ShiftModifier | Qt::Key_F1)));
  EXPECT_TRUE(GlobalShortcutsManager::NormalizeKeySequence(QKeySequence()).isEmpty());

}

// The application wide shortcut of the play/pause key also has the key which Qt reports for it on Windows.
TEST(GlobalShortcutsManagerTest, ApplicationKeySequencesAddsTogglePlayPause) {

  EXPECT_EQ(QList<QKeySequence>() << QKeySequence(Qt::Key_MediaPlay) << QKeySequence(Qt::Key_MediaTogglePlayPause), GlobalShortcutsManager::ApplicationKeySequences(QKeySequence(Qt::Key_MediaPlay)));
  EXPECT_EQ(QList<QKeySequence>() << QKeySequence(Qt::ControlModifier | Qt::Key_MediaPlay) << QKeySequence(Qt::ControlModifier | Qt::Key_MediaTogglePlayPause), GlobalShortcutsManager::ApplicationKeySequences(QKeySequence(Qt::ControlModifier | Qt::Key_MediaPlay)));
  EXPECT_EQ(QList<QKeySequence>() << QKeySequence(Qt::Key_MediaNext), GlobalShortcutsManager::ApplicationKeySequences(QKeySequence(Qt::Key_MediaNext)));
  EXPECT_EQ(QList<QKeySequence>() << QKeySequence(), GlobalShortcutsManager::ApplicationKeySequences(QKeySequence()));

}

// A shortcut saved with the play/pause key before it was normalized is the same key sequence as the default Play/Pause shortcut when it's loaded, so the settings find it when the key is used for another action.
TEST(GlobalShortcutsManagerTest, SavedTogglePlayPauseIsLoadedAsMediaPlay) {

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  // The native backend is always used on macOS and Windows, it registers the shortcuts with the system, and on macOS it needs the application which is set up by Strawberry before QApplication.
  // Loading the shortcuts is the same on all platforms, so it's tested on the other platforms, where the backends are disabled in the settings.
  GTEST_SKIP() << "The native global shortcuts backend can't be disabled on this platform.";
#endif

  {
    Settings s;
    s.beginGroup(GlobalShortcutsSettings::kSettingsGroup);
    // The shortcuts are not registered with the desktop by the test.
    s.setValue(GlobalShortcutsSettings::kUseKGlobalAccel, false);
    s.setValue(GlobalShortcutsSettings::kUseX11, false);
    s.setValue(u"next_track"_s, QKeySequence(Qt::Key_MediaTogglePlayPause).toString());
    s.endGroup();
  }

  GlobalShortcutsManager manager;
  EXPECT_EQ(QKeySequence(Qt::Key_MediaPlay), manager.shortcuts().value(u"next_track"_s).action->shortcut());
  EXPECT_EQ(QKeySequence(Qt::Key_MediaPlay), manager.shortcuts().value(u"play_pause"_s).action->shortcut());
  EXPECT_EQ(QList<QKeySequence>() << QKeySequence(Qt::Key_MediaPlay) << QKeySequence(Qt::Key_MediaTogglePlayPause), manager.shortcuts().value(u"next_track"_s).shortcut->keys());

  {
    Settings s;
    s.remove(QLatin1String(GlobalShortcutsSettings::kSettingsGroup));
  }

}

}  // namespace
