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

import android.Manifest;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONObject;

// Called from the Qt alarm clock page through JNI. The alarms are stored on the Java side, so they ring and are rescheduled while the Qt app is not running.
public final class AlarmBridge {

  static final int PERMISSION_NOTIFICATIONS = AlarmPermissions.NOTIFICATIONS;
  static final int PERMISSION_EXACT_ALARMS = AlarmPermissions.EXACT_ALARMS;
  static final int PERMISSION_FULL_SCREEN = AlarmPermissions.FULL_SCREEN;
  static final int PERMISSION_CHANNEL = AlarmPermissions.CHANNEL;
  static final int PERMISSION_OVERLAY = AlarmPermissions.OVERLAY;

  private static final String PERMISSION_PREFERENCES = "alarm_permissions";
  private static final String KEY_NOTIFICATIONS_ASKED = "notifications_asked";
  private static final int REQUEST_NOTIFICATIONS = 2801;

  private static boolean notificationsAskedThisSession = false;

  private AlarmBridge() {}

  public static void init(final Context context, final String labels) {

    AlarmStore.setLabels(context, labels);
    AlarmStore.reschedule(context);

  }

  public static String getAlarms(final Context context) {
    return AlarmStore.alarms(context).toString();
  }

  public static boolean saveAlarm(final Context context, final String json) {

    try {
      final JSONObject alarm = new JSONObject(json);
      final JSONObject stored = AlarmStore.alarm(context, alarm.optString("id"));
      final int tracks = trackCount(alarm);
      // Logged, since the Qt app's own log does not reach logcat on Android.
      if (stored == null || trackCount(stored) != tracks) Log.i(AlarmStore.TAG, "Alarm songs updated: " + tracks + " song(s)");
      AlarmStore.putAlarm(context, alarm);
      return true;
    }
    catch (Exception e) {
      Log.e(AlarmStore.TAG, "Could not save an alarm", e);
      return false;
    }

  }

  private static int trackCount(final JSONObject alarm) {

    final org.json.JSONArray tracks = alarm.optJSONArray("tracks");
    return tracks == null ? 0 : tracks.length();

  }

  public static void deleteAlarm(final Context context, final String id) {
    AlarmStore.removeAlarm(context, id);
  }

  public static int getAutoStopMinutes(final Context context) {
    return AlarmStore.autoStopMinutes(context);
  }

  public static void setAutoStopMinutes(final Context context, final int minutes) {
    AlarmStore.setAutoStopMinutes(context, minutes);
  }

  public static long nextAlarmMillis(final Context context) {
    return AlarmStore.nextAlarm(context);
  }

  public static long snoozeUntil(final Context context, final String id) {
    return AlarmStore.snoozeUntil(context, id);
  }

  // Returns the PERMISSION_ bits of the permissions and settings that alarms need but that are not granted.
  public static int missingPermissions(final Context context) {

    final NotificationManager notifications = context.getSystemService(NotificationManager.class);
    final boolean notificationsAllowed = notificationPermission(context) && notifications.areNotificationsEnabled();
    final boolean exactAlarms = Build.VERSION.SDK_INT < Build.VERSION_CODES.S || context.getSystemService(AlarmManager.class).canScheduleExactAlarms();
    final boolean fullScreen = Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE || notifications.canUseFullScreenIntent();
    return AlarmPermissions.missing(notificationsAllowed, blockedAlarmChannel(context) != null, exactAlarms, fullScreen, Settings.canDrawOverlays(context));

  }

  // Asks for the notification permission if Android still shows its dialog; the other settings can only be changed in the system settings.
  public static void requestRuntimePermissions(final Context context) {

    if (!canRequestNotifications(context)) return;
    notificationsAskedThisSession = true;
    context.getSharedPreferences(PERMISSION_PREFERENCES, Context.MODE_PRIVATE).edit().putBoolean(KEY_NOTIFICATIONS_ASKED, true).apply();
    try {
      ((Activity) context).requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, REQUEST_NOTIFICATIONS);
    }
    catch (RuntimeException e) {
      Log.e(AlarmStore.TAG, "Could not request the notification permission", e);
    }

  }

  public static void requestPermission(final Context context, final int permission) {

    try {
      if (permission == PERMISSION_NOTIFICATIONS) {
        // The button asks again even if the dialog was dismissed in this session.
        notificationsAskedThisSession = false;
        if (canRequestNotifications(context)) {
          requestRuntimePermissions(context);
        }
        else {
          start(context, new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName()));
        }
      }
      else if (permission == PERMISSION_CHANNEL) {
        final String channel = blockedAlarmChannel(context);
        start(context, new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName()).putExtra(Settings.EXTRA_CHANNEL_ID, channel == null ? AlarmService.CHANNEL_ID : channel));
      }
      else if (permission == PERMISSION_EXACT_ALARMS && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        start(context, new Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, Uri.parse("package:" + context.getPackageName())));
      }
      else if (permission == PERMISSION_FULL_SCREEN && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
        start(context, new Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT, Uri.parse("package:" + context.getPackageName())));
      }
      else if (permission == PERMISSION_OVERLAY) {
        start(context, new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + context.getPackageName())));
      }
    }
    catch (Exception e) {
      Log.e(AlarmStore.TAG, "Could not request permission " + permission, e);
    }

  }

  // Opens the ringing screen if an alarm is ringing, for example when the user opens Strawberry because no notification appeared.
  public static void showRingingScreen(final Context context) {

    if (!AlarmService.isRinging()) return;
    try {
      start(context, new Intent(context, AlarmActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
    }
    catch (RuntimeException e) {
      Log.w(AlarmStore.TAG, "Could not open the ringing screen", e);
    }

  }

  private static boolean canRequestNotifications(final Context context) {

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || !(context instanceof Activity)) return false;
    final boolean asked = context.getSharedPreferences(PERMISSION_PREFERENCES, Context.MODE_PRIVATE).getBoolean(KEY_NOTIFICATIONS_ASKED, false);
    final boolean rationale = ((Activity) context).shouldShowRequestPermissionRationale(Manifest.permission.POST_NOTIFICATIONS);
    return AlarmPermissions.shouldRequestNotifications(notificationPermission(context), asked, rationale, notificationsAskedThisSession);

  }

  private static boolean notificationPermission(final Context context) {
    return Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED;
  }

  // Returns the ID of an alarm notification channel that the user switched off, or null.
  private static String blockedAlarmChannel(final Context context) {

    final NotificationManager notifications = context.getSystemService(NotificationManager.class);
    // Created now, so its settings page exists before the first alarm rings.
    AlarmService.createChannel(context);
    for (final String id : new String[] { AlarmService.CHANNEL_ID, AlarmReceiver.FALLBACK_CHANNEL_ID }) {
      final NotificationChannel channel = notifications.getNotificationChannel(id);
      if (channel != null && channel.getImportance() == NotificationManager.IMPORTANCE_NONE) return id;
    }
    return null;

  }

  public static void openClock(final Context context) {
    start(context, new Intent(context, ClockActivity.class));
  }

  private static void start(final Context context, final Intent intent) {

    if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);

  }

}
