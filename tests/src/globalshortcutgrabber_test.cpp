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

#include "gtest_include.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QString>
#include <QTimer>

#include "globalshortcuts/globalshortcutgrabber.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Records the key with the grabber, the key is pressed when the dialog is shown.
QKeySequence RecordKey(const Qt::Key key) {

  GlobalShortcutGrabber grabber;
  QTimer::singleShot(0, &grabber, [&grabber, key]() {
    QKeyEvent event(QEvent::ShortcutOverride, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&grabber, &event);
  });
  // The dialog is closed when the key isn't accepted, so the test doesn't hang.
  QTimer::singleShot(5000, &grabber, &GlobalShortcutGrabber::reject);
  return grabber.GetKey(u"Play/Pause"_s);

}

// Qt reports the play/pause media key as Key_MediaTogglePlayPause on Windows, it must be recorded as the key of the default Play/Pause shortcut, so the settings find another action which uses the same key.
TEST(GlobalShortcutGrabberTest, TogglePlayPauseIsRecordedAsMediaPlay) {

  EXPECT_EQ(QKeySequence(Qt::Key_MediaPlay), RecordKey(Qt::Key_MediaTogglePlayPause));

}

TEST(GlobalShortcutGrabberTest, OtherKeysAreRecorded) {

  EXPECT_EQ(QKeySequence(Qt::Key_MediaNext), RecordKey(Qt::Key_MediaNext));
  EXPECT_EQ(QKeySequence(Qt::Key_MediaPlay), RecordKey(Qt::Key_MediaPlay));

}

}  // namespace
