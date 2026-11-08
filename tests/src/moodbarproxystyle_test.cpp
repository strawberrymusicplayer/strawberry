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

#include <QByteArray>
#include <QImage>
#include <QSlider>
#include <QTimeLine>

#include "moodbar/moodbarproxystyle.h"

namespace {

// Moodbar data with the colors changing along the moodbar, reversed for the other moodbar, so the moodbars look different.
QByteArray MoodbarData(const bool reversed) {

  QByteArray data;
  constexpr int kSamples = 1000;
  for (int i = 0; i < kSamples; ++i) {
    const int value = (reversed ? kSamples - 1 - i : i) * 255 / (kSamples - 1);
    data.append(static_cast<char>(value));
    data.append(static_cast<char>(255 - value));
    data.append(static_cast<char>((value * 3) % 256));
  }
  return data;

}

class MoodbarProxyStyleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    slider_ = std::make_unique<QSlider>(Qt::Horizontal);
    slider_->resize(400, 30);
    slider_->show();
    style_ = std::make_unique<MoodbarProxyStyle>(&*slider_);
    style_->SetShowMoodbar(true);
    timeline_ = style_->findChild<QTimeLine*>();
    ASSERT_NE(nullptr, timeline_);
  }

  void TearDown() override {
    // The style owns the fade timeline, delete it while the slider is still alive.
    style_.reset();
    slider_.reset();
  }

  // Moves the fade to the given time and keeps it there, the image is grabbed at the same point of the fade for each case.
  QImage GrabAt(const int msec) {
    if (timeline_->state() == QTimeLine::State::Running) timeline_->setPaused(true);
    timeline_->setCurrentTime(msec);
    return slider_->grab().toImage();
  }

  // Shows the first moodbar, fades it out partly, then sets the next moodbar, like a quick track change, and returns the image while fading.
  QImage QuickTrackChange(const QByteArray &first_data, const QByteArray &next_data) {

    style_->SetMoodbarData(first_data);
    // The fade is finished when the moodbar is drawn at the end of the fade.
    GrabAt(timeline_->duration());

    style_->SetMoodbarData(QByteArray());
    GrabAt(600);

    style_->SetMoodbarData(next_data);
    return GrabAt(600);

  }

  std::unique_ptr<QSlider> slider_;
  std::unique_ptr<MoodbarProxyStyle> style_;
  QTimeLine *timeline_ = nullptr;
};

// Regression test: on a quick track change, the fade continued with the moodbar of the previous song, and then jumped to the new one.
TEST_F(MoodbarProxyStyleTest, QuickTrackChangeFadesToNewMoodbar) {

  const QImage image = QuickTrackChange(MoodbarData(false), MoodbarData(true));

  // The same fade, where the previous moodbar is the same as the new one.
  TearDown();
  SetUp();
  const QImage expected_image = QuickTrackChange(MoodbarData(true), MoodbarData(true));

  // The other moodbar looks different while fading at the same point.
  TearDown();
  SetUp();
  const QImage previous_moodbar_image = QuickTrackChange(MoodbarData(false), MoodbarData(false));
  ASSERT_NE(expected_image, previous_moodbar_image);

  EXPECT_EQ(expected_image, image);

}

}  // namespace
