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

// Decides whether an alarm sounds again when Android restarts AlarmService after the process ended.
// Android delays such a restart by at least twice the time the alarm already sounded, on a clock that stops while the phone sleeps, so it can come back long after the alarm should have stopped.
final class AlarmRestart {

  // How long an alarm may still sound again after a restart when the automatic stop is off.
  static final long MAXIMUM_AGE_MSEC = 120L * 60000L;

  private AlarmRestart() {}

  // Returns how many milliseconds the alarm may still sound, or 0 if it must not sound again.
  // soundingId and wentOffElapsed are the stored alarm that sounds and the elapsedRealtime() when it went off; Stop, Snooze and the automatic stop clear them.
  static long remainingMsec(final String alarmId, final String soundingId, final long wentOffElapsed, final long nowElapsed, final int autoStopMinutes) {

    if (alarmId == null || !alarmId.equals(soundingId) || wentOffElapsed <= 0) return 0;
    final long age = nowElapsed - wentOffElapsed;
    if (age < 0) return 0;
    final long limit = autoStopMinutes > 0 ? autoStopMinutes * 60000L : MAXIMUM_AGE_MSEC;
    return Math.max(0, limit - age);

  }

}
