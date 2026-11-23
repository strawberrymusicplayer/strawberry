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

// Tests AlarmHandover.java on a desktop JVM without the Android SDK.
public final class AlarmHandoverTest {

  private static int failures = 0;

  private AlarmHandoverTest() {}

  public static void main(final String[] args) {

    AlarmHandover handover = new AlarmHandover();
    // The app is running: ask it first, silently for a moment.
    expect("running app is asked", handover.start("a", true, true) == (AlarmHandover.SEND_REQUEST | AlarmHandover.SCHEDULE_GRACE | AlarmHandover.SCHEDULE_TIMEOUT));
    expect("claim before grace", handover.claimed("a") == 0 && handover.state() == AlarmHandover.QT && !handover.javaSound());
    expect("repeated claim is ignored", handover.claimed("a") == 0);
    expect("grace after claim does nothing", handover.graceEnded() == 0);
    expect("timeout after claim does nothing", handover.timeout() == 0 && handover.state() == AlarmHandover.QT);
    expect("qt stops playing", handover.failed("a") == AlarmHandover.START_JAVA_SOUND && handover.state() == AlarmHandover.JAVA);
    expect("no second java sound", handover.failed("a") == 0);
    expect("late claim is stopped", handover.claimed("a") == AlarmHandover.STOP_QT && handover.state() == AlarmHandover.JAVA);
    expect("stop", handover.finish() == 0 && handover.state() == AlarmHandover.IDLE);

    handover = new AlarmHandover();
    handover.start("b", true, true);
    expect("grace starts the java sound", handover.graceEnded() == AlarmHandover.START_JAVA_SOUND && handover.javaSound());
    expect("claim stops java only after the ack", handover.claimed("b") == AlarmHandover.STOP_JAVA_SOUND && !handover.javaSound());
    expect("stop while qt plays stops qt", handover.finish() == AlarmHandover.STOP_QT);

    handover = new AlarmHandover();
    // The app is not running: start it and play the Java sound until it takes over.
    expect("closed app is started", handover.start("c", true, false) == (AlarmHandover.START_APP | AlarmHandover.START_JAVA_SOUND | AlarmHandover.SCHEDULE_TIMEOUT));
    expect("claim before the request is stopped", handover.claimed("c") == AlarmHandover.STOP_QT && handover.state() == AlarmHandover.WAITING);
    expect("ready sends the request and restarts the timeout", handover.qtReady() == (AlarmHandover.SEND_REQUEST | AlarmHandover.SCHEDULE_TIMEOUT));
    expect("ready twice sends once", handover.qtReady() == 0);
    expect("stale token is stopped", handover.claimed("other") == AlarmHandover.STOP_QT && handover.state() == AlarmHandover.WAITING);
    expect("stale failure is ignored", handover.failed("other") == 0 && handover.state() == AlarmHandover.WAITING);
    expect("timeout keeps java and cancels qt", handover.timeout() == AlarmHandover.STOP_QT && handover.state() == AlarmHandover.JAVA);
    expect("claim after timeout is stopped", handover.claimed("c") == AlarmHandover.STOP_QT);
    expect("ready after timeout does nothing", handover.qtReady() == 0);
    expect("stop after timeout", handover.finish() == 0);

    handover = new AlarmHandover();
    handover.start("d", true, false);
    expect("timeout before the app is ready", handover.timeout() == 0 && handover.state() == AlarmHandover.JAVA);

    handover = new AlarmHandover();
    handover.start("e", true, true);
    expect("failure before claim plays java", handover.failed("e") == AlarmHandover.START_JAVA_SOUND && handover.state() == AlarmHandover.JAVA);
    expect("silent timeout plays java once", handover.timeout() == 0);

    handover = new AlarmHandover();
    handover.start("f", true, true);
    expect("silent timeout plays java", handover.timeout() == (AlarmHandover.STOP_QT | AlarmHandover.START_JAVA_SOUND));

    handover = new AlarmHandover();
    expect("default sound stays in java", handover.start("g", false, true) == AlarmHandover.START_JAVA_SOUND && handover.state() == AlarmHandover.JAVA);
    expect("ready does not ask qt", handover.qtReady() == 0);

    handover = new AlarmHandover();
    handover.start("h", true, true);
    handover.claimed("h");
    expect("second alarm stops qt first", (handover.start("i", true, true) & AlarmHandover.STOP_QT) != 0 && handover.token().equals("i"));
    expect("old token can't claim the new alarm", handover.claimed("h") == AlarmHandover.STOP_QT && handover.state() == AlarmHandover.WAITING);
    expect("stop while waiting cancels the request", handover.finish() == AlarmHandover.STOP_QT);
    expect("claim after stop is stopped", handover.claimed("i") == AlarmHandover.STOP_QT);

    // The alarm screen starts the app that is not running yet, once per alarm, and comes back once the player is ready.
    handover = new AlarmHandover();
    handover.start("j", true, false);
    expect("running app is not started", !handover.startAppFromScreen(true, false));
    expect("app is not started behind a secure lock screen", !handover.startAppFromScreen(false, true));
    expect("screen starts the app", handover.startAppFromScreen(false, false));
    expect("app is started once", !handover.startAppFromScreen(false, false));
    expect("screen waits for the player", !handover.takeScreenAfterAppStart(false));
    handover.qtReady();
    expect("screen returns when the player is ready", handover.takeScreenAfterAppStart(true));
    expect("screen returns once", !handover.takeScreenAfterAppStart(true));

    handover = new AlarmHandover();
    handover.start("k", true, false);
    handover.startAppFromScreen(false, false);
    handover.timeout();
    expect("screen returns after the time-out", handover.takeScreenAfterAppStart(false));

    // Stop or Snooze while the app starts must not bring the alarm screen back.
    handover = new AlarmHandover();
    handover.start("l", true, false);
    handover.startAppFromScreen(false, false);
    handover.finish();
    expect("no screen after stop", !handover.takeScreenAfterAppStart(false) && !handover.takeScreenAfterAppStart(true));
    handover.start("m", true, false);
    expect("next alarm starts the app again", handover.startAppFromScreen(false, false));

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
