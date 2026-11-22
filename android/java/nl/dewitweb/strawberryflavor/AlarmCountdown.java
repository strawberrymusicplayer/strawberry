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

// The minute countdown to a snoozed alarm, shared by the clock screen; plain Java, so it is tested on a desktop JVM.
final class AlarmCountdown {

  static final long MINUTE_MSEC = 60000L;

  private AlarmCountdown() {}

  // Whole minutes left, rounded up, so the last minute shows 1 rather than 0.
  static long minutesLeft(final long now, final long until) {
    return until <= now ? 0 : (until - now + MINUTE_MSEC - 1) / MINUTE_MSEC;
  }

  // The delay until the next wall-clock minute or, if sooner, until the countdown to the time changes.
  static long refreshDelay(final long now, final long until) {

    long delay = MINUTE_MSEC - Math.floorMod(now, MINUTE_MSEC);
    if (until > now) {
      final long change = (until - now) % MINUTE_MSEC;
      delay = Math.min(delay, change == 0 ? MINUTE_MSEC : change);
    }
    return delay;

  }

  // Fills in a translated template such as "Snoozed until %1, goes off in %2 min".
  static String snoozeText(final String template, final String time, final long minutes) {
    return template.replace("%1", time).replace("%2", Long.toString(minutes));
  }

}
