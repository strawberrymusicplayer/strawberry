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

#include <memory>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QString>
#include <QList>
#include <QTimer>
#include <QGraphicsOpacityEffect>

#include "core/settings.h"
#include "constants/notificationssettings.h"
#include "osd/osdpretty.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class OSDPrettyTest : public ::testing::Test {
 protected:
  void SetUp() override {

    Settings s;
    s.beginGroup(OSDPrettySettings::kSettingsGroup);
    s.setValue(OSDPrettySettings::kFading, true);
    s.setValue(OSDPrettySettings::kDisableDuration, false);
    s.endGroup();

    osd_ = std::make_unique<OSDPretty>(OSDPretty::Mode::Popup);
    ASSERT_TRUE(osd_->fading());

  }

  // Processes events until the condition is true, returns false on timeout.
  template <typename Condition>
  static bool WaitFor(Condition condition, const int timeout_msec = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > timeout_msec) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
  }

  // Processes events for the time.
  static void Wait(const int msec) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < msec) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
  }

  // Returns the opacity of the OSD, on Wayland with layer shell the opacity is set with a graphics effect instead of the window opacity.
  qreal Opacity() const {
    QGraphicsOpacityEffect *opacity_effect = qobject_cast<QGraphicsOpacityEffect*>(osd_->graphicsEffect());
    return opacity_effect ? opacity_effect->opacity() : osd_->windowOpacity();
  }

  // Returns the timer which hides the OSD after the popup duration.
  QTimer *PopupTimer() const {
    const QList<QTimer*> timers = osd_->findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly);
    return timers.count() == 1 ? timers.first() : nullptr;
  }

  // Shows the OSD, waits for it to fade in, then starts fading it out.
  void ShowAndStartFadeOut() {
    osd_->ShowMessage(u"Summary 1"_s, u"Message 1"_s, QImage());
    ASSERT_TRUE(WaitFor([this]() { return Opacity() >= 1.0; }));
    osd_->hide();
    // The fade-out takes 300 ms, the OSD is still visible part way through it.
    Wait(100);
    ASSERT_TRUE(osd_->isVisible());
    ASSERT_LT(Opacity(), 1.0);
  }

  std::unique_ptr<OSDPretty> osd_;
};

TEST_F(OSDPrettyTest, MessageDuringFadeOutIsShown) {

  osd_->set_popup_duration(5000);
  ShowAndStartFadeOut();

  osd_->ShowMessage(u"Summary 2"_s, u"Message 2"_s, QImage());

  // Well past the end of the fade-out, the OSD is shown again at full opacity.
  Wait(600);
  EXPECT_TRUE(osd_->isVisible());
  EXPECT_EQ(Opacity(), 1.0);

}

TEST_F(OSDPrettyTest, MessageDuringFadeOutIsHiddenAfterDurationAndShownAgain) {

  osd_->set_popup_duration(5000);
  ShowAndStartFadeOut();

  osd_->set_popup_duration(200);
  osd_->ShowMessage(u"Summary 2"_s, u"Message 2"_s, QImage());

  // The popup duration starts again when the OSD has faded back in, and then it fades out and is hidden.
  ASSERT_TRUE(WaitFor([this]() { return Opacity() >= 1.0; }));
  EXPECT_TRUE(osd_->isVisible());
  ASSERT_TRUE(WaitFor([this]() { return !osd_->isVisible(); }));

  // A message after the fade-out has finished shows the OSD again.
  osd_->ShowMessage(u"Summary 3"_s, u"Message 3"_s, QImage());
  EXPECT_TRUE(osd_->isVisible());

}

// Hiding the OSD again while it fades out hides it at once, a message then shows it again.
TEST_F(OSDPrettyTest, MessageAfterSecondHideDuringFadeOutIsShown) {

  osd_->set_popup_duration(5000);
  ShowAndStartFadeOut();

  osd_->hide();
  ASSERT_FALSE(osd_->isVisible());

  osd_->ShowMessage(u"Summary 2"_s, u"Message 2"_s, QImage());
  EXPECT_TRUE(osd_->isVisible());
  EXPECT_TRUE(WaitFor([this]() { return Opacity() >= 1.0; }));
  EXPECT_TRUE(osd_->isVisible());

}

// Clicking the OSD fades it out while the timeout of the message is still running, it must not fade out the next message while it fades in.
TEST_F(OSDPrettyTest, MessageAfterClickDuringFadeOutStopsPreviousTimeout) {

  QTimer *popup_timer = PopupTimer();
  ASSERT_TRUE(popup_timer);

  osd_->set_popup_duration(5000);
  ShowAndStartFadeOut();
  ASSERT_TRUE(popup_timer->isActive());

  osd_->ShowMessage(u"Summary 2"_s, u"Message 2"_s, QImage());
  EXPECT_FALSE(popup_timer->isActive());

  // The timeout is started again when the OSD has faded back in.
  ASSERT_TRUE(WaitFor([this]() { return Opacity() >= 1.0; }));
  EXPECT_TRUE(WaitFor([popup_timer]() { return popup_timer->isActive(); }));

}

}  // namespace
