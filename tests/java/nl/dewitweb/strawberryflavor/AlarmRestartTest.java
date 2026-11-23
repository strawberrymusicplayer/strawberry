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

// Tests AlarmRestart.java, which decides whether a restarted alarm sounds again, on a desktop JVM without the Android SDK.
public final class AlarmRestartTest {

  private static final long MINUTE = 60000L;
  private static int failures = 0;

  private AlarmRestartTest() {}

  public static void main(final String[] args) {

    final long wentOff = 1000L * MINUTE;

    expect("sounds for the rest of the automatic stop", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff + 4 * MINUTE, 30) == 26 * MINUTE);
    expect("does not sound past the automatic stop", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff + 30 * MINUTE, 30) == 0);
    expect("does not sound long after the automatic stop", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff + 90 * MINUTE, 30) == 0);
    expect("without automatic stop, sounds within the maximum age", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff + 100 * MINUTE, 0) == 20 * MINUTE);
    expect("without automatic stop, not past the maximum age", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff + AlarmRestart.MAXIMUM_AGE_MSEC, 0) == 0);
    expect("a stopped or snoozed alarm does not sound", AlarmRestart.remainingMsec("a", null, 0, wentOff, 30) == 0);
    expect("a replaced alarm does not sound", AlarmRestart.remainingMsec("a", "b", wentOff, wentOff + MINUTE, 30) == 0);
    expect("a time from before a reboot does not sound", AlarmRestart.remainingMsec("a", "a", wentOff, wentOff - MINUTE, 30) == 0);
    expect("a missing alarm id does not sound", AlarmRestart.remainingMsec(null, "a", wentOff, wentOff + MINUTE, 30) == 0);

    if (failures > 0) {
      System.err.println(failures + " check(s) failed");
      System.exit(1);
    }
    System.out.println("All checks passed");

  }

  private static void expect(final String name, final boolean condition) {
    if (!condition) {
      System.err.println("FAILED: " + name);
      ++failures;
    }
  }

}
