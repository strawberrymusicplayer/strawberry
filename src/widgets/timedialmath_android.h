// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMEDIALMATH_ANDROID_H
#define TIMEDIALMATH_ANDROID_H

// The geometry and value conversions of the Android time dial, without any widget code, so they can be tested natively.
// Angles are in degrees, clockwise from the 12 o'clock position, in [0, 360).
// Positions are offsets from the dial's center in screen coordinates, so y grows downwards.
// Hours are 0 to 23 throughout; the 12-hour clock is only a way to show and pick them.
namespace TimeDialMath {

constexpr int kPositions = 12;
constexpr double kHourStep = 360.0 / kPositions;
constexpr double kMinuteStep = 360.0 / 60;

double AngleFromOffset(const double dx, const double dy);

// The number position, 0 at the top to 11, nearest to the angle.
int PositionFromAngle(const double angle);
double AngleOfPosition(const int position);

// Returns the minute nearest to the angle.
int MinuteFromAngle(const double angle);
double AngleOfMinute(const int minute);

// Whether a touch at the distance from the center picks from the inner ring of the 24-hour dial.
// The boundary lies halfway between the rings.
bool IsInnerRing(const double distance, const double outer_radius, const double inner_radius);

// The 24-hour dial shows 12 and 1 to 11 on the outer ring and 00 and 13 to 23 on the inner ring, like Android's.
// The 12-hour dial shows 12 and 1 to 11; the AM or PM choice decides the hour.
int HourFromPosition(const int position, const bool use_24_hour, const bool inner_ring, const bool pm);
int PositionOfHour(const int hour);
bool IsInnerRingHour(const int hour);

bool IsPm(const int hour);
// 1 to 12.
int To12Hour(const int hour);
int From12Hour(const int hour_12, const bool pm);
// Keeps the hour on the clock face and switches between AM and PM.
int WithPm(const int hour, const bool pm);

// Returns the hour from the dial at the angle and distance from the center.
int HourAt(const double angle, const double distance, const double outer_radius, const double inner_radius, const bool use_24_hour, const bool pm);

}  // namespace TimeDialMath

#endif  // TIMEDIALMATH_ANDROID_H
