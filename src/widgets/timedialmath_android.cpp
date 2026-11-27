// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>

#include "timedialmath_android.h"

namespace TimeDialMath {

namespace {

constexpr double kPi = 3.14159265358979323846;

double NormalizedAngle(const double angle) {

  const double normalized = std::fmod(angle, 360.0);
  return normalized < 0.0 ? normalized + 360.0 : normalized;

}

int Modulo(const int value, const int divisor) {
  return ((value % divisor) + divisor) % divisor;
}

}  // namespace

double AngleFromOffset(const double dx, const double dy) {
  return NormalizedAngle(std::atan2(dx, -dy) * 180.0 / kPi);
}

int PositionFromAngle(const double angle) {
  return Modulo(static_cast<int>(std::lround(NormalizedAngle(angle) / kHourStep)), kPositions);
}

double AngleOfPosition(const int position) {
  return Modulo(position, kPositions) * kHourStep;
}

int MinuteFromAngle(const double angle) {
  return Modulo(static_cast<int>(std::lround(NormalizedAngle(angle) / kMinuteStep)), 60);
}

double AngleOfMinute(const int minute) {
  return Modulo(minute, 60) * kMinuteStep;
}

bool IsInnerRing(const double distance, const double outer_radius, const double inner_radius) {
  return distance < (outer_radius + inner_radius) / 2.0;
}

int HourFromPosition(const int position, const bool use_24_hour, const bool inner_ring, const bool pm) {

  const int p = Modulo(position, kPositions);
  if (use_24_hour) {
    if (inner_ring) return p == 0 ? 0 : p + 12;
    return p == 0 ? 12 : p;
  }
  return From12Hour(p == 0 ? 12 : p, pm);

}

int PositionOfHour(const int hour) {
  return Modulo(hour, kPositions);
}

bool IsInnerRingHour(const int hour) {

  const int h = Modulo(hour, 24);
  return h == 0 || h > 12;

}

bool IsPm(const int hour) {
  return Modulo(hour, 24) >= 12;
}

int To12Hour(const int hour) {

  const int h = Modulo(hour, 12);
  return h == 0 ? 12 : h;

}

int From12Hour(const int hour_12, const bool pm) {
  return Modulo(hour_12, 12) + (pm ? 12 : 0);
}

int WithPm(const int hour, const bool pm) {
  return From12Hour(To12Hour(hour), pm);
}

int HourAt(const double angle, const double distance, const double outer_radius, const double inner_radius, const bool use_24_hour, const bool pm) {
  return HourFromPosition(PositionFromAngle(angle), use_24_hour, use_24_hour && IsInnerRing(distance, outer_radius, inner_radius), pm);
}

}  // namespace TimeDialMath
