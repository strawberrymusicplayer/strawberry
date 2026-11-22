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

// Tests ClockFace.java on a desktop JVM without the Android SDK.
public final class ClockFaceTest {

  private static int failures = 0;

  private ClockFaceTest() {}

  public static void main(final String[] args) {

    expect("noon hour hand", ClockFace.hourAngle(12, 0, 0) == 0.0F);
    expect("3 PM hour hand", ClockFace.hourAngle(15, 0, 0) == 90.0F);
    expect("hour hand moves with the minutes", ClockFace.hourAngle(9, 30, 0) == 285.0F);
    expect("hour hand moves with the seconds", ClockFace.hourAngle(0, 0, 60) == 0.5F);
    expect("minute hand", ClockFace.minuteAngle(45, 0) == 270.0F);
    expect("minute hand moves with the seconds", ClockFace.minuteAngle(10, 30) == 63.0F);
    expect("second hand", ClockFace.secondAngle(15) == 90.0F);
    expect("digital is the default", ClockFace.clockType(7) == ClockFace.DIGITAL);
    expect("analog", ClockFace.clockType(ClockFace.ANALOG) == ClockFace.ANALOG);
    expect("system brightness", ClockFace.windowBrightness(0) == ClockFace.SYSTEM_BRIGHTNESS);
    expect("5 percent", ClockFace.windowBrightness(5) == 0.05F);
    expect("full brightness", ClockFace.windowBrightness(100) == 1.0F);
    expect("capped at full", ClockFace.windowBrightness(150) == 1.0F);

    if (failures > 0) {
      System.err.println(failures + " test(s) failed");
      System.exit(1);
    }
    System.out.println("All tests passed");

  }

  private static void expect(final String name, final boolean condition) {

    if (condition) return;
    System.err.println("FAILED: " + name);
    ++failures;

  }

}
