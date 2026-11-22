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

// Decides which permissions and settings an alarm that goes off is missing, without Android classes, so it can be tested on a desktop JVM.
final class AlarmPermissions {

  // Must match the kPermission constants in alarmbridge_android.h.
  static final int NOTIFICATIONS = 1;
  static final int EXACT_ALARMS = 2;
  static final int FULL_SCREEN = 4;
  static final int CHANNEL = 8;
  static final int OVERLAY = 16;

  private AlarmPermissions() {}

  // notificationsAllowed: the runtime permission (always true before Android 13) and the app's notification switch.
  // channelBlocked: the user switched off the alarm notification category.
  // overlayAllowed: "Display over other apps", which lets the alarm screen open from the background when notifications cannot.
  static int missing(final boolean notificationsAllowed, final boolean channelBlocked, final boolean exactAlarmsAllowed, final boolean fullScreenAllowed, final boolean overlayAllowed) {

    int missing = 0;
    if (!notificationsAllowed) missing |= NOTIFICATIONS;
    // The category switch only matters while the app's notifications are on.
    else if (channelBlocked) missing |= CHANNEL;
    if (!exactAlarmsAllowed) missing |= EXACT_ALARMS;
    if (!fullScreenAllowed) missing |= FULL_SCREEN;
    // Full-screen intents are part of the notification, so without notifications only the overlay permission can open the alarm screen from the background.
    if ((missing & (NOTIFICATIONS | CHANNEL)) != 0 && !overlayAllowed) missing |= OVERLAY;
    return missing;

  }

  // Android shows the notification permission dialog at most twice; after the second denial only the app settings can grant it.
  // asked: requested before, in any session; rationale: Android would show the dialog again.
  static boolean shouldRequestNotifications(final boolean granted, final boolean asked, final boolean rationale, final boolean askedThisSession) {
    return !granted && !askedThisSession && (!asked || rationale);
  }

}
