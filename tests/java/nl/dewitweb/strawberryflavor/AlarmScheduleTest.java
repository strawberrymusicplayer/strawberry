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

import java.time.ZoneId;
import java.time.ZonedDateTime;

// Tests AlarmSchedule.java, the scheduling used on Android, on a desktop JVM without the Android SDK.
public final class AlarmScheduleTest {

  private static final int WEEKDAYS = 0x1f;
  private static final ZoneId AMSTERDAM = ZoneId.of("Europe/Amsterdam");
  private static final ZoneId SANTIAGO = ZoneId.of("America/Santiago");

  private static int failures = 0;

  private AlarmScheduleTest() {}

  public static void main(final String[] args) {

    // A weekday alarm on Friday after its time goes off on Monday, and before its time the same day.
    expect("repeating alarm skips to the next selected day", trigger(true, WEEKDAYS, 7, 0, local(AMSTERDAM, 2026, 11, 20, 8, 0)), local(AMSTERDAM, 2026, 11, 23, 7, 0));
    expect("repeating alarm goes off later today", trigger(true, WEEKDAYS, 7, 0, local(AMSTERDAM, 2026, 11, 20, 6, 59)), local(AMSTERDAM, 2026, 11, 20, 7, 0));
    // Sunday only, at the current minute: next week.
    expect("repeating alarm on one day waits a week", trigger(true, 0x40, 9, 30, local(AMSTERDAM, 2026, 11, 22, 9, 30)), local(AMSTERDAM, 2026, 11, 29, 9, 30));
    expect("one-off alarm in the past goes off tomorrow", trigger(true, 0, 6, 45, local(AMSTERDAM, 2026, 11, 22, 15, 40)), local(AMSTERDAM, 2026, 11, 23, 6, 45));
    expect("one-off alarm later today goes off today", trigger(true, 0, 6, 45, local(AMSTERDAM, 2026, 11, 22, 6, 44)), local(AMSTERDAM, 2026, 11, 22, 6, 45));
    expect("one-off alarm crosses the year", trigger(true, 0, 6, 45, local(AMSTERDAM, 2026, 12, 31, 23, 0)), local(AMSTERDAM, 2027, 1, 1, 6, 45));
    expectEquals("disabled alarm does not go off", AlarmSchedule.nextTrigger(false, 0, 6, 0, local(AMSTERDAM, 2026, 11, 22, 5, 0)), -1);
    expect("out of range time is clamped", trigger(true, 0, 30, -5, local(AMSTERDAM, 2026, 11, 22, 5, 0)), local(AMSTERDAM, 2026, 11, 22, 23, 0));

    // Amsterdam skips 02:00 to 03:00 on 2026-03-29: 02:30 goes off at 03:30 CEST, 01:30 UTC.
    expectEquals("time in a spring forward gap goes off after the gap", trigger(true, 0, 2, 30, local(AMSTERDAM, 2026, 3, 28, 23, 0)), utc(2026, 3, 29, 1, 30));
    expectEquals("time after the gap is unchanged", trigger(true, 0, 7, 0, local(AMSTERDAM, 2026, 3, 28, 23, 0)), utc(2026, 3, 29, 5, 0));
    // Amsterdam repeats 02:00 to 03:00 on 2026-10-25: 02:30 goes off once, at the first occurrence, 00:30 UTC.
    expectEquals("time repeated by fall back goes off the first time", trigger(true, 0x7f, 2, 30, ZonedDateTime.of(2026, 10, 24, 22, 0, 0, 0, ZoneId.of("UTC")).withZoneSameInstant(AMSTERDAM)), utc(2026, 10, 25, 0, 30));
    expectEquals("time repeated by fall back does not go off twice", trigger(true, 0x7f, 2, 30, ZonedDateTime.of(2026, 10, 25, 0, 31, 0, 0, ZoneId.of("UTC")).withZoneSameInstant(AMSTERDAM)), utc(2026, 10, 26, 1, 30));
    // Santiago skips midnight to 01:00 on 2026-09-06: 00:30 goes off at 01:30 that day, not on the previous evening.
    expect("time in a midnight gap goes off after the gap", trigger(true, 0, 0, 30, local(SANTIAGO, 2026, 9, 5, 22, 0)), local(SANTIAGO, 2026, 9, 6, 1, 30));

    if (failures > 0) {
      System.out.println(failures + " test(s) failed");
      System.exit(1);
    }
    System.out.println("All AlarmSchedule tests passed");

  }

  private static long trigger(final boolean enabled, final int days, final int hour, final int minute, final ZonedDateTime now) {
    return AlarmSchedule.nextTrigger(enabled, days, hour, minute, now);
  }

  private static ZonedDateTime local(final ZoneId zone, final int year, final int month, final int day, final int hour, final int minute) {
    return ZonedDateTime.of(year, month, day, hour, minute, 0, 0, zone);
  }

  private static long utc(final int year, final int month, final int day, final int hour, final int minute) {
    return ZonedDateTime.of(year, month, day, hour, minute, 0, 0, ZoneId.of("UTC")).toInstant().toEpochMilli();
  }

  private static void expect(final String name, final long actual, final ZonedDateTime expected) {
    expectEquals(name, actual, expected.toInstant().toEpochMilli());
  }

  private static void expectEquals(final String name, final long actual, final long expected) {

    if (actual == expected) {
      System.out.println("PASS " + name);
      return;
    }
    ++failures;
    System.out.println("FAIL " + name + ": expected " + expected + ", got " + actual);

  }

}
