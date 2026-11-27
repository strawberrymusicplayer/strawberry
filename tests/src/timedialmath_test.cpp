// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include "widgets/timedialmath_android.h"

using namespace TimeDialMath;

namespace {

TEST(TimeDialMathTest, AngleIsClockwiseFromTwelve) {

  EXPECT_DOUBLE_EQ(AngleFromOffset(0, -10), 0.0);
  EXPECT_DOUBLE_EQ(AngleFromOffset(10, 0), 90.0);
  EXPECT_DOUBLE_EQ(AngleFromOffset(0, 10), 180.0);
  EXPECT_DOUBLE_EQ(AngleFromOffset(-10, 0), 270.0);
  EXPECT_NEAR(AngleFromOffset(-1, -1000), 360.0, 0.1);
  EXPECT_LT(AngleFromOffset(-1, -1000), 360.0);

}

TEST(TimeDialMathTest, PositionsSnapToNearestNumber) {

  EXPECT_EQ(PositionFromAngle(0.0), 0);
  EXPECT_EQ(PositionFromAngle(14.9), 0);
  EXPECT_EQ(PositionFromAngle(15.1), 1);
  EXPECT_EQ(PositionFromAngle(90.0), 3);
  EXPECT_EQ(PositionFromAngle(345.1), 0);
  EXPECT_EQ(PositionFromAngle(-20.0), 11);
  EXPECT_DOUBLE_EQ(AngleOfPosition(3), 90.0);
  EXPECT_DOUBLE_EQ(AngleOfPosition(12), 0.0);

}

TEST(TimeDialMathTest, MinutesSnapToNearestMinute) {

  EXPECT_EQ(MinuteFromAngle(0.0), 0);
  EXPECT_EQ(MinuteFromAngle(2.9), 0);
  EXPECT_EQ(MinuteFromAngle(3.1), 1);
  EXPECT_EQ(MinuteFromAngle(42.0), 7);
  EXPECT_EQ(MinuteFromAngle(180.0), 30);
  EXPECT_EQ(MinuteFromAngle(357.5), 0);
  EXPECT_EQ(MinuteFromAngle(353.0), 59);
  for (int minute = 0; minute < 60; ++minute) EXPECT_EQ(MinuteFromAngle(AngleOfMinute(minute)), minute);

}

TEST(TimeDialMathTest, TwentyFourHourRings) {

  EXPECT_EQ(HourFromPosition(0, true, false, false), 12);
  EXPECT_EQ(HourFromPosition(5, true, false, true), 5);
  EXPECT_EQ(HourFromPosition(0, true, true, false), 0);
  EXPECT_EQ(HourFromPosition(5, true, true, false), 17);
  EXPECT_EQ(HourFromPosition(11, true, true, false), 23);
  for (int hour = 0; hour < 24; ++hour) {
    EXPECT_EQ(HourFromPosition(PositionOfHour(hour), true, IsInnerRingHour(hour), false), hour);
  }

}

TEST(TimeDialMathTest, InnerRingBoundaryIsHalfway) {

  EXPECT_TRUE(IsInnerRing(69.0, 100.0, 40.0));
  EXPECT_FALSE(IsInnerRing(71.0, 100.0, 40.0));
  // Straight up, near the inner ring is 00, near the outer ring 12; at the right 15 and 3.
  EXPECT_EQ(HourAt(0.0, 40.0, 100.0, 40.0, true, false), 0);
  EXPECT_EQ(HourAt(0.0, 100.0, 100.0, 40.0, true, false), 12);
  EXPECT_EQ(HourAt(90.0, 45.0, 100.0, 40.0, true, false), 15);
  EXPECT_EQ(HourAt(90.0, 95.0, 100.0, 40.0, true, false), 3);
  // The 12-hour dial has no inner ring.
  EXPECT_EQ(HourAt(90.0, 40.0, 100.0, 40.0, false, false), 3);

}

TEST(TimeDialMathTest, TwelveHourConversion) {

  EXPECT_EQ(To12Hour(0), 12);
  EXPECT_EQ(To12Hour(12), 12);
  EXPECT_EQ(To12Hour(13), 1);
  EXPECT_EQ(To12Hour(23), 11);
  EXPECT_FALSE(IsPm(0));
  EXPECT_FALSE(IsPm(11));
  EXPECT_TRUE(IsPm(12));
  EXPECT_TRUE(IsPm(23));
  EXPECT_EQ(From12Hour(12, false), 0);
  EXPECT_EQ(From12Hour(12, true), 12);
  EXPECT_EQ(From12Hour(7, true), 19);
  EXPECT_EQ(HourFromPosition(0, false, false, false), 0);
  EXPECT_EQ(HourFromPosition(0, false, false, true), 12);
  EXPECT_EQ(HourFromPosition(7, false, true, true), 19);
  EXPECT_EQ(WithPm(7, true), 19);
  EXPECT_EQ(WithPm(19, false), 7);
  EXPECT_EQ(WithPm(0, true), 12);
  EXPECT_EQ(WithPm(12, false), 0);
  for (int hour = 0; hour < 24; ++hour) EXPECT_EQ(From12Hour(To12Hour(hour), IsPm(hour)), hour);

}

}  // namespace
