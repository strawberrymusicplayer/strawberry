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

// Tests AlarmCountdown.java on a desktop JVM without the Android SDK.
public final class AlarmCountdownTest {

  private static int failures = 0;

  private AlarmCountdownTest() {}

  public static void main(final String[] args) {

    final long minute = AlarmCountdown.MINUTE_MSEC;
    // 10:00:00 plus 15 seconds, as epoch milliseconds aligned to a minute.
    final long now = 1000 * minute + 15000;
    expect("6.5 minutes round up to 7", AlarmCountdown.minutesLeft(now, now + 6 * minute + 30000) == 7);
    expect("exactly 7 minutes", AlarmCountdown.minutesLeft(now, now + 7 * minute) == 7);
    expect("last second shows 1", AlarmCountdown.minutesLeft(now, now + 1000) == 1);
    expect("past shows 0", AlarmCountdown.minutesLeft(now, now - 1000) == 0);
    expect("no snooze waits for the next minute", AlarmCountdown.refreshDelay(now, 0) == 45000);
    expect("countdown change comes first", AlarmCountdown.refreshDelay(now, now + 6 * minute + 30000) == 30000);
    expect("minute change comes first", AlarmCountdown.refreshDelay(now, now + 6 * minute + 50000) == 45000);
    expect("whole minutes left wait a minute at most", AlarmCountdown.refreshDelay(now, now + 7 * minute) == 45000);
    expect("delay is reached after the change", AlarmCountdown.minutesLeft(now + 30000, now + 6 * minute + 30000) == 6);
    expect("text", AlarmCountdown.snoozeText("Snoozed until %1, goes off in %2 min", "Sun 10:45", 7).equals("Snoozed until Sun 10:45, goes off in 7 min"));

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
