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

// Decides who plays a sounding alarm: the Qt app through its own player, or AlarmService with the local copies or the default sound.
// Exactly one of them owns the sound; AlarmService stops its own sound only after the Qt app has acknowledged that it plays.
// Without Android classes, so it is tested on a desktop JVM; AlarmService runs the timers and carries out the returned actions.
final class AlarmHandover {

  // How long the Qt app may take to play the alarm once asked; an app that has to be started first gets this long to start as well.
  static final long TIMEOUT_MSEC = 20000;
  // How long AlarmService waits silently for an app that is already running before it plays its own sound.
  static final long GRACE_MSEC = 3000;

  // Bits of the actions AlarmService must carry out, in this order.
  static final int START_APP = 1;
  static final int SEND_REQUEST = 2;
  static final int START_JAVA_SOUND = 4;
  static final int STOP_JAVA_SOUND = 8;
  static final int STOP_QT = 16;
  static final int SCHEDULE_GRACE = 32;
  static final int SCHEDULE_TIMEOUT = 64;

  // Constants rather than an enum, see AlarmFallback.
  static final int IDLE = 0;
  // Waiting for the Qt app to acknowledge; AlarmService may already play its own sound.
  static final int WAITING = 1;
  // The Qt app plays the alarm.
  static final int QT = 2;
  // AlarmService plays the alarm until it is stopped or snoozed; the Qt app is not asked again.
  static final int JAVA = 3;

  private int state = IDLE;
  private String token;
  private boolean requestSent = false;
  private boolean javaSound = false;
  // The token of the alarm for which the alarm screen started the app, and whether the alarm screen must come back afterwards.
  private String appStartToken;
  private boolean screenAfterAppStart = false;

  int state() {
    return state;
  }

  String token() {
    return token;
  }

  boolean javaSound() {
    return javaSound;
  }

  // The alarm goes off. qtSupported is false for sources the Qt app does not play, such as the default sound.
  int start(final String newToken, final boolean qtSupported, final boolean qtReady) {

    int actions = 0;
    // A second alarm replaces the first one, which the Qt app may still play.
    if (requestSent && (state == WAITING || state == QT)) actions |= STOP_QT;
    token = newToken;
    requestSent = false;
    javaSound = false;
    screenAfterAppStart = false;
    if (!qtSupported) {
      state = JAVA;
      javaSound = true;
      return actions | START_JAVA_SOUND;
    }
    state = WAITING;
    actions |= SCHEDULE_TIMEOUT;
    if (qtReady) {
      requestSent = true;
      return actions | SEND_REQUEST | SCHEDULE_GRACE;
    }
    // Starting the app takes too long to wait silently.
    javaSound = true;
    return actions | START_APP | START_JAVA_SOUND;

  }

  // The Qt app finished starting after the alarm went off; the time-out starts again for playing.
  int qtReady() {

    if (state != WAITING || requestSent) return 0;
    requestSent = true;
    return SEND_REQUEST | SCHEDULE_TIMEOUT;

  }

  // The alarm screen is shown; returns true once per alarm if it must start the app, which is not running yet.
  // The app then stays in front while it starts, and takeScreenAfterAppStart() tells when the alarm screen must come back.
  boolean startAppFromScreen(final boolean playerReady) {

    if (state != WAITING || playerReady || token.equals(appStartToken)) return false;
    appStartToken = token;
    screenAfterAppStart = true;
    return true;

  }

  // Returns true once when the alarm screen must come back after starting the app: the app's player is ready or the app is no longer awaited.
  // Never after Stop, Snooze or the automatic stop, which end the alarm.
  boolean takeScreenAfterAppStart(final boolean playerReady) {

    if (!screenAfterAppStart || state == IDLE || (state == WAITING && !playerReady)) return false;
    screenAfterAppStart = false;
    return true;

  }

  int graceEnded() {

    if (state != WAITING || javaSound) return 0;
    javaSound = true;
    return START_JAVA_SOUND;

  }

  int timeout() {

    if (state != WAITING) return 0;
    state = JAVA;
    int actions = requestSent ? STOP_QT : 0;
    if (!javaSound) {
      javaSound = true;
      actions |= START_JAVA_SOUND;
    }
    return actions;

  }

  // The Qt app acknowledged that the alarm plays. A late or unknown acknowledgement is answered with a stop, so the alarm never plays twice.
  int claimed(final String claimedToken) {

    if (state == QT && claimedToken.equals(token)) return 0;
    if (state != WAITING || !requestSent || !claimedToken.equals(token)) return STOP_QT;
    state = QT;
    if (!javaSound) return 0;
    javaSound = false;
    return STOP_JAVA_SOUND;

  }

  // The Qt app could not play the alarm, or stopped playing it, for example when the NAS was unreachable or the songs ended.
  int failed(final String failedToken) {

    if (!failedToken.equals(token) || (state != WAITING && state != QT)) return 0;
    state = JAVA;
    if (javaSound) return 0;
    javaSound = true;
    return START_JAVA_SOUND;

  }

  // Stop, Snooze or the automatic stop.
  int finish() {

    final int actions = requestSent && (state == WAITING || state == QT) ? STOP_QT : 0;
    state = IDLE;
    requestSent = false;
    javaSound = false;
    screenAfterAppStart = false;
    return actions;

  }

}
