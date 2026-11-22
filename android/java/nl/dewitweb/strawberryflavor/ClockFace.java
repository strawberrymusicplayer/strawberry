/*
 * Strawberry Music Player
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

package nl.dewitweb.strawberryflavor;

// The clock settings and the angles of the analog clock's hands, without Android dependencies so they can be tested on a desktop JVM.
final class ClockFace {

  static final int DIGITAL = 0;
  static final int ANALOG = 1;

  // Leaves the brightness to the system, like WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE.
  static final float SYSTEM_BRIGHTNESS = -1.0F;

  private ClockFace() {}

  // Angles in degrees clockwise from 12 o'clock; the hands move smoothly rather than jumping.
  static float hourAngle(final int hour, final int minute, final int second) {
    return (hour % 12) * 30.0F + minute * 0.5F + second / 120.0F;
  }

  static float minuteAngle(final int minute, final int second) {
    return minute * 6.0F + second * 0.1F;
  }

  static float secondAngle(final int second) {
    return second * 6.0F;
  }

  static int minuteOfDay(final int hour, final int minute) {
    return hour * 60 + minute;
  }

  static int clockType(final int stored) {
    return stored == ANALOG ? ANALOG : DIGITAL;
  }

  // Maps the brightness setting in percent to the window brightness; 0 or less keeps the system brightness.
  static float windowBrightness(final int percent) {
    if (percent <= 0) return SYSTEM_BRIGHTNESS;
    return Math.min(percent, 100) / 100.0F;
  }

}
