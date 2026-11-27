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

// Tests AlarmPermissions.java, which decides which alarm permission warnings the overview shows, on a desktop JVM without the Android SDK.
public final class AlarmPermissionsTest {

  private static int failures = 0;

  private AlarmPermissionsTest() {}

  public static void main(final String[] args) {

    expect("everything allowed", AlarmPermissions.missing(true, false, true, true, false) == 0);
    expect("notifications off also asks for the overlay", AlarmPermissions.missing(false, false, true, true, false) == (AlarmPermissions.NOTIFICATIONS | AlarmPermissions.OVERLAY));
    expect("notifications off with the overlay allowed", AlarmPermissions.missing(false, false, true, true, true) == AlarmPermissions.NOTIFICATIONS);
    expect("blocked channel also asks for the overlay", AlarmPermissions.missing(true, true, true, true, false) == (AlarmPermissions.CHANNEL | AlarmPermissions.OVERLAY));
    expect("blocked channel is not reported while all notifications are off", (AlarmPermissions.missing(false, true, true, true, true) & AlarmPermissions.CHANNEL) == 0);
    expect("full screen and exact alarms", AlarmPermissions.missing(true, false, false, false, false) == (AlarmPermissions.EXACT_ALARMS | AlarmPermissions.FULL_SCREEN));

    expect("never asked", AlarmPermissions.shouldRequestNotifications(false, false, false, false));
    expect("granted", !AlarmPermissions.shouldRequestNotifications(true, false, false, false));
    expect("denied once, Android asks again", AlarmPermissions.shouldRequestNotifications(false, true, true, false));
    expect("denied for good", !AlarmPermissions.shouldRequestNotifications(false, true, false, false));
    expect("once per session", !AlarmPermissions.shouldRequestNotifications(false, false, false, true));

    if (failures > 0) {
      System.out.println(failures + " test(s) failed");
      System.exit(1);
    }
    System.out.println("All tests passed");

  }

  private static void expect(final String name, final boolean condition) {

    if (condition) return;
    System.out.println("FAILED: " + name);
    ++failures;

  }

}
