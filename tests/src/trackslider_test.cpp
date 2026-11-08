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

#include <QString>
#include <QLabel>
#include <QPoint>
#include <QPointF>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QApplication>

#include "widgets/tracksliderslider.h"
#include "widgets/tracksliderpopup.h"

#include "core/settings.h"
#include "widgets/trackslider.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class TrackSliderTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    // The remaining time, or the length when false.
    Settings s;
    s.beginGroup(u"MainWindow"_s);
    s.setValue(u"show_remaining_time"_s, GetParam());
    s.endGroup();
    track_slider_ = std::make_unique<TrackSlider>();
    remaining_ = track_slider_->findChild<QLabel*>(u"remaining"_s);
    elapsed_ = track_slider_->findChild<QLabel*>(u"elapsed"_s);
    ASSERT_NE(nullptr, remaining_);
    ASSERT_NE(nullptr, elapsed_);
  }

  std::unique_ptr<TrackSlider> track_slider_;
  QLabel *remaining_ = nullptr;
  QLabel *elapsed_ = nullptr;
};

// Regression test: streams have no length, the remaining time was the time elapsed, counting up as a negative time.
TEST_P(TrackSliderTest, UnknownLengthShowsNoTime) {

  track_slider_->SetValue(12000, 0);
  EXPECT_EQ(u"0:12"_s, elapsed_->text());
  EXPECT_EQ(QString(), remaining_->text()) << remaining_->text().toStdString();

  track_slider_->SetValue(13000, 0);
  EXPECT_EQ(QString(), remaining_->text());

}

TEST_P(TrackSliderTest, KnownLengthShowsTime) {

  track_slider_->SetValue(5000, 60000);
  EXPECT_EQ(u"0:05"_s, elapsed_->text());
  EXPECT_EQ(GetParam() ? u"-0:55"_s : u"1:00"_s, remaining_->text());

}

// After a stream, the time is shown again for the next song, the length is only updated when it changes while the song can be seeked.
TEST_P(TrackSliderTest, KnownLengthAfterUnknownLengthShowsTime) {

  track_slider_->SetCanSeek(true);
  track_slider_->SetValue(5000, 60000);
  track_slider_->SetCanSeek(false);
  track_slider_->SetValue(12000, 0);
  track_slider_->SetCanSeek(true);
  track_slider_->SetValue(1000, 60000);
  EXPECT_EQ(GetParam() ? u"-0:59"_s : u"1:00"_s, remaining_->text());

}

// Moves the mouse over the slider, after entering it when enter is set, and returns whether the time popup is shown.
bool HoverShowsPopup(TrackSlider *track_slider, const bool enter = true) {

  TrackSliderSlider *slider = track_slider->findChild<TrackSliderSlider*>(u"slider"_s);
  TrackSliderPopup *popup = track_slider->findChild<TrackSliderPopup*>();
  EXPECT_NE(nullptr, slider);
  EXPECT_NE(nullptr, popup);
  if (!slider || !popup) return false;

  const QPointF pos(slider->width() / 2.0, slider->height() / 2.0);
  if (enter) {
    QEnterEvent enter_event(pos, slider->mapToGlobal(pos), slider->mapToGlobal(pos));
    QApplication::sendEvent(slider, &enter_event);
  }
  QMouseEvent move_event(QEvent::MouseMove, pos, slider->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(slider, &move_event);

  return popup->isVisible();

}

// Regression test: streams have no length, hovering showed 0:00, and the time elapsed as a negative time from there.
TEST_P(TrackSliderTest, UnknownLengthShowsNoPopup) {

  track_slider_->resize(400, 30);
  track_slider_->show();
  track_slider_->SetCanSeek(true);

  track_slider_->SetValue(12000, 0);
  EXPECT_FALSE(HoverShowsPopup(&*track_slider_));

}

// The popup is shown for songs with a length, and hidden when hovering after the song changes to a stream.
TEST_P(TrackSliderTest, KnownLengthShowsPopup) {

  track_slider_->resize(400, 30);
  track_slider_->show();
  track_slider_->SetCanSeek(true);

  track_slider_->SetValue(5000, 60000);
  EXPECT_TRUE(HoverShowsPopup(&*track_slider_));

  track_slider_->SetValue(12000, 0);
  EXPECT_FALSE(HoverShowsPopup(&*track_slider_));

}

// The popup is shown again when the song changes from a stream to a song with a length while hovering, without the mouse entering again.
TEST_P(TrackSliderTest, LengthWhileHoveringShowsPopup) {

  track_slider_->resize(400, 30);
  track_slider_->show();
  track_slider_->SetCanSeek(true);

  // The mouse enters while there's no length.
  track_slider_->SetValue(12000, 0);
  EXPECT_FALSE(HoverShowsPopup(&*track_slider_));

  track_slider_->SetValue(1000, 60000);
  EXPECT_TRUE(HoverShowsPopup(&*track_slider_, false));

  // The popup was hidden when moving while there was no length.
  track_slider_->SetValue(12000, 0);
  EXPECT_FALSE(HoverShowsPopup(&*track_slider_, false));
  track_slider_->SetValue(1000, 60000);
  EXPECT_TRUE(HoverShowsPopup(&*track_slider_, false));

}

// The popup is hidden when the song changes to a stream while the mouse is still over the slider, without the mouse moving.
TEST_P(TrackSliderTest, UnknownLengthWhileHoveringHidesPopup) {

  track_slider_->resize(400, 30);
  track_slider_->show();
  track_slider_->SetCanSeek(true);

  track_slider_->SetValue(5000, 60000);
  EXPECT_TRUE(HoverShowsPopup(&*track_slider_));

  TrackSliderPopup *popup = track_slider_->findChild<TrackSliderPopup*>();
  ASSERT_NE(nullptr, popup);

  track_slider_->SetValue(12000, 0);
  EXPECT_FALSE(popup->isVisible());

}

INSTANTIATE_TEST_SUITE_P(RemainingTimeAndLength, TrackSliderTest, ::testing::Values(true, false));

}  // namespace
