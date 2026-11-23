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

#include <limits>

#include "gtest_include.h"

#include <QtGlobal>

#include "constants/timeconstants.h"
#include "mpris2/mpris2.h"

namespace {

constexpr qint64 kLengthNanosec = 180 * kNsecPerSec;

// Regression test: the position was rounded down to whole seconds, so at 10.9 seconds, seeking forward by half a second went to 10 seconds.
TEST(Mpris2Test, SeekPositionIsNotRoundedToSeconds) {

  EXPECT_EQ(10'400'000'000ULL, Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, -500'000LL));
  EXPECT_EQ(11'400'000'000ULL, Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, 500'000LL));
  EXPECT_EQ(10'900'001'000ULL, Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, 1LL));

}

TEST(Mpris2Test, SeekPositionIsLimitedToTheSong) {

  // Seeking back before the start seeks to the start, and seeking forward past the end seeks to the end.
  EXPECT_EQ(0ULL, Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, -11 * kUsecPerSec));
  EXPECT_EQ(static_cast<quint64>(kLengthNanosec), Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, 200 * kUsecPerSec));

  // The offset comes from a D-Bus client, the largest offsets don't overflow.
  EXPECT_EQ(static_cast<quint64>(kLengthNanosec), Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, std::numeric_limits<qint64>::max()));
  EXPECT_EQ(0ULL, Mpris2::SeekPositionNanosec(10'900'000'000LL, kLengthNanosec, std::numeric_limits<qint64>::min()));

}

}  // namespace
