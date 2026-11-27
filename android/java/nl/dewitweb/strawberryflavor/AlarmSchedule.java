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

import java.time.DayOfWeek;
import java.time.LocalDate;
import java.time.LocalTime;
import java.time.ZonedDateTime;

// Calculates when an alarm goes off; the single implementation used for scheduling, also by the Qt app through AlarmBridge.
// It uses only java.time, so tests/java/AlarmScheduleTest.java can test it on a desktop JVM.
final class AlarmSchedule {

  private AlarmSchedule() {}

  // Returns the next time after now when the alarm goes off in milliseconds since the epoch, or -1 if it is disabled.
  // Days of the week have Monday as bit 0; no days means the alarm goes off once, today if its time is still ahead, otherwise tomorrow.
  static long nextTrigger(final boolean enabled, final int days, final int hour, final int minute, final ZonedDateTime now) {

    if (!enabled) return -1;
    final int repeatDays = days & 0x7f;
    final LocalTime time = LocalTime.of(Math.max(0, Math.min(23, hour)), Math.max(0, Math.min(59, minute)));
    final LocalDate today = now.toLocalDate();
    for (int i = 0; i <= 7; ++i) {
      final LocalDate date = today.plusDays(i);
      final DayOfWeek day = date.getDayOfWeek();
      if (repeatDays != 0 && (repeatDays & (1 << (day.getValue() - 1))) == 0) continue;
      // ZonedDateTime.of() moves a time in a daylight saving gap later by the length of the gap and uses the earlier of two occurrences.
      final ZonedDateTime trigger = ZonedDateTime.of(date, time, now.getZone());
      if (trigger.isAfter(now)) return trigger.toInstant().toEpochMilli();
    }
    return -1;

  }

}
