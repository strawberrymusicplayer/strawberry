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
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONObject;

// Called from the Qt alarm clock page through JNI. The alarms are stored on the Java side, so they ring and are rescheduled while the Qt app is not running.
public final class AlarmBridge {

  static final int PERMISSION_NOTIFICATIONS = 1;
  static final int PERMISSION_EXACT_ALARMS = 2;
  static final int PERMISSION_FULL_SCREEN = 4;

  private static final String PERMISSION_PREFERENCES = "alarm_permissions";
  private static final String KEY_NOTIFICATIONS_ASKED = "notifications_asked";
  private static final int REQUEST_NOTIFICATIONS = 2801;

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

  // Returns the PERMISSION_ bits of the permissions that alarms need but that are not granted.
  public static int missingPermissions(final Context context) {

    int missing = 0;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
      missing |= PERMISSION_NOTIFICATIONS;
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && !context.getSystemService(AlarmManager.class).canScheduleExactAlarms()) {
      missing |= PERMISSION_EXACT_ALARMS;
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && !context.getSystemService(NotificationManager.class).canUseFullScreenIntent()) {
      missing |= PERMISSION_FULL_SCREEN;
    }
    return missing;

  }

  public static void requestPermission(final Context context, final int permission) {

    try {
      if (permission == PERMISSION_NOTIFICATIONS && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        final SharedPreferences preferences = context.getSharedPreferences(PERMISSION_PREFERENCES, Context.MODE_PRIVATE);
        // Android shows the permission dialog only until it is denied, after that only the app settings can grant it.
        if (context instanceof Activity && !preferences.getBoolean(KEY_NOTIFICATIONS_ASKED, false)) {
          preferences.edit().putBoolean(KEY_NOTIFICATIONS_ASKED, true).apply();
          ((Activity) context).requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, REQUEST_NOTIFICATIONS);
        }
        else {
          start(context, new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName()));
        }
      }
      else if (permission == PERMISSION_EXACT_ALARMS && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        start(context, new Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, Uri.parse("package:" + context.getPackageName())));
      }
      else if (permission == PERMISSION_FULL_SCREEN && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
        start(context, new Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT, Uri.parse("package:" + context.getPackageName())));
      }
    }
    catch (Exception e) {
      Log.e(AlarmStore.TAG, "Could not request permission " + permission, e);
    }

  }

  public static void openClock(final Context context) {
    start(context, new Intent(context, ClockActivity.class));
  }

  private static void start(final Context context, final Intent intent) {

    if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);

  }

}
