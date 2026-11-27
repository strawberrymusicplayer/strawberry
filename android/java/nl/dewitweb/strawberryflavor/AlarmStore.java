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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.HashSet;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Keeps the alarm clock's alarms in SharedPreferences and schedules them with AlarmManager, so they go off and survive a reboot while the Qt app is not running.
// The alarms are a JSON array in the format of Alarm::ToJson() in alarmschedule_android.cpp; nextTrigger() must match AlarmSchedule::NextTrigger().
final class AlarmStore {

  static final String TAG = "StrawberryAlarm";
  static final String ACTION_FIRE = "nl.dewitweb.strawberryflavor.ALARM_FIRE";
  static final String EXTRA_ID = "id";
  static final String EXTRA_SNOOZE = "snooze";
  static final int DEFAULT_AUTO_STOP_MINUTES = 30;

  private static final String PREFERENCES = "alarm_clock";
  private static final String KEY_ALARMS = "alarms";
  private static final String KEY_AUTO_STOP = "auto_stop_minutes";
  private static final String KEY_PREVIOUS_ALARM_VOLUME = "previous_alarm_volume";
  private static final String KEY_SOUNDING_ID = "sounding_id";
  private static final String KEY_SOUNDING_SINCE = "sounding_since";
  private static final String KEY_CLOCK_TYPE = "clock_type";
  private static final String KEY_CLOCK_BRIGHTNESS = "clock_brightness";
  private static final String KEY_SNOOZE_PREFIX = "snooze_";
  private static final String KEY_SCHEDULED = "scheduled";
  private static final String KEY_LABELS = "labels";

  // Alarms are changed from the Qt thread, the receivers and the alarm service.
  private static final Object lock = new Object();

  private AlarmStore() {}

  private static SharedPreferences preferences(final Context context) {
    return context.getApplicationContext().getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
  }

  static JSONArray alarms(final Context context) {

    synchronized (lock) {
      try {
        return new JSONArray(preferences(context).getString(KEY_ALARMS, "[]"));
      }
      catch (JSONException e) {
        Log.e(TAG, "Could not read the alarms", e);
        return new JSONArray();
      }
    }

  }

  static JSONObject alarm(final Context context, final String id) {

    final JSONArray list = alarms(context);
    for (int i = 0; i < list.length(); ++i) {
      final JSONObject alarm = list.optJSONObject(i);
      if (alarm != null && id.equals(alarm.optString("id"))) return alarm;
    }
    return null;

  }

  // Adds the alarm or replaces the alarm with the same ID; switching an alarm off also stops snoozing.
  // A one-off alarm is already off while it snoozes after going off, so saving it keeps the snooze.
  static void putAlarm(final Context context, final JSONObject alarm) {

    synchronized (lock) {
      final String id = alarm.optString("id");
      if (id.isEmpty()) return;
      final JSONArray list = alarms(context);
      final JSONArray updated = new JSONArray();
      boolean replaced = false;
      boolean switchedOff = false;
      for (int i = 0; i < list.length(); ++i) {
        final JSONObject existing = list.optJSONObject(i);
        if (existing == null) continue;
        if (id.equals(existing.optString("id"))) {
          updated.put(alarm);
          replaced = true;
          switchedOff = existing.optBoolean("enabled", true) && !alarm.optBoolean("enabled", true);
        }
        else {
          updated.put(existing);
        }
      }
      if (!replaced) updated.put(alarm);
      final SharedPreferences.Editor editor = preferences(context).edit().putString(KEY_ALARMS, updated.toString());
      if (switchedOff) editor.remove(KEY_SNOOZE_PREFIX + id);
      editor.commit();
    }
    reschedule(context);

  }

  static void removeAlarm(final Context context, final String id) {

    synchronized (lock) {
      final JSONArray list = alarms(context);
      final JSONArray updated = new JSONArray();
      for (int i = 0; i < list.length(); ++i) {
        final JSONObject existing = list.optJSONObject(i);
        if (existing != null && !id.equals(existing.optString("id"))) updated.put(existing);
      }
      preferences(context).edit().putString(KEY_ALARMS, updated.toString()).remove(KEY_SNOOZE_PREFIX + id).commit();
    }
    reschedule(context);

  }

  static void disableAlarm(final Context context, final String id) {

    synchronized (lock) {
      final JSONObject alarm = alarm(context, id);
      if (alarm == null) return;
      try {
        alarm.put("enabled", false);
      }
      catch (JSONException e) {
        return;
      }
      final JSONArray list = alarms(context);
      final JSONArray updated = new JSONArray();
      for (int i = 0; i < list.length(); ++i) {
        final JSONObject existing = list.optJSONObject(i);
        if (existing != null) updated.put(id.equals(existing.optString("id")) ? alarm : existing);
      }
      preferences(context).edit().putString(KEY_ALARMS, updated.toString()).commit();
    }

  }

  // Called when an alarm goes off: a snoozed alarm stops snoozing and a one-off alarm is switched off, snoozing it still goes off again.
  static void consumeAlarm(final Context context, final JSONObject alarm, final boolean snoozed) {

    final String id = alarm.optString("id");
    if (snoozed) {
      setSnooze(context, id, 0);
    }
    else if ((alarm.optInt("days", 0) & 0x7f) == 0) {
      disableAlarm(context, id);
    }
    reschedule(context);

  }

  // The alarm volume from before a sounding alarm changed it, kept here so it can be restored when the process ended while the alarm sounded; -1 if none.
  static int previousAlarmVolume(final Context context) {
    return preferences(context).getInt(KEY_PREVIOUS_ALARM_VOLUME, -1);
  }

  static void setPreviousAlarmVolume(final Context context, final int volume) {
    preferences(context).edit().putInt(KEY_PREVIOUS_ALARM_VOLUME, volume).commit();
  }

  // The alarm that sounds and the SystemClock.elapsedRealtime() when it went off, so a restart after the process ended knows whether it may still sound; null and 0 if none.
  static String soundingId(final Context context) {
    return preferences(context).getString(KEY_SOUNDING_ID, null);
  }

  static long soundingSince(final Context context) {
    return preferences(context).getLong(KEY_SOUNDING_SINCE, 0);
  }

  static void setSounding(final Context context, final String id, final long since) {
    preferences(context).edit().putString(KEY_SOUNDING_ID, id).putLong(KEY_SOUNDING_SINCE, since).commit();
  }

  static int autoStopMinutes(final Context context) {
    return preferences(context).getInt(KEY_AUTO_STOP, DEFAULT_AUTO_STOP_MINUTES);
  }

  static void setAutoStopMinutes(final Context context, final int minutes) {
    preferences(context).edit().putInt(KEY_AUTO_STOP, Math.max(0, minutes)).commit();
  }

  // ClockFace.DIGITAL or ClockFace.ANALOG, for Tools > Clock and the alarm screen.
  static int clockType(final Context context) {
    return ClockFace.clockType(preferences(context).getInt(KEY_CLOCK_TYPE, ClockFace.DIGITAL));
  }

  static void setClockType(final Context context, final int type) {
    preferences(context).edit().putInt(KEY_CLOCK_TYPE, ClockFace.clockType(type)).commit();
  }

  // The screen brightness in percent while Tools > Clock is shown, or 0 for the system brightness.
  static int clockBrightness(final Context context) {
    return preferences(context).getInt(KEY_CLOCK_BRIGHTNESS, 0);
  }

  static void setClockBrightness(final Context context, final int percent) {
    preferences(context).edit().putInt(KEY_CLOCK_BRIGHTNESS, Math.max(0, Math.min(percent, 100))).commit();
  }

  static long snoozeUntil(final Context context, final String id) {
    return preferences(context).getLong(KEY_SNOOZE_PREFIX + id, 0);
  }

  static void setSnooze(final Context context, final String id, final long until) {

    synchronized (lock) {
      final SharedPreferences.Editor editor = preferences(context).edit();
      if (until > 0) editor.putLong(KEY_SNOOZE_PREFIX + id, until);
      else editor.remove(KEY_SNOOZE_PREFIX + id);
      editor.commit();
    }
    reschedule(context);

  }

  // Translated texts from the Qt app, kept so the alarm screen and the clock use them while the Qt app is not running.
  static void setLabels(final Context context, final String labels) {
    preferences(context).edit().putString(KEY_LABELS, labels).commit();
  }

  static String label(final Context context, final String key, final String fallback) {

    try {
      final String value = new JSONObject(preferences(context).getString(KEY_LABELS, "{}")).optString(key);
      return value.isEmpty() ? fallback : value;
    }
    catch (JSONException e) {
      return fallback;
    }

  }

  // Returns the next time the alarm goes off after now in milliseconds since the epoch, or -1 if it is disabled.
  static long nextTrigger(final JSONObject alarm, final ZonedDateTime now) {
    return AlarmSchedule.nextTrigger(alarm.optBoolean("enabled", true), alarm.optInt("days", 0), alarm.optInt("hour", 7), alarm.optInt("minute", 0), now);
  }

  // Returns the next time any alarm or snoozed alarm goes off in milliseconds since the epoch, or -1.
  static long nextAlarm(final Context context) {

    final ZonedDateTime now = ZonedDateTime.now(ZoneId.systemDefault());
    final long nowMillis = now.toInstant().toEpochMilli();
    long next = -1;
    final JSONArray list = alarms(context);
    for (int i = 0; i < list.length(); ++i) {
      final JSONObject alarm = list.optJSONObject(i);
      if (alarm == null) continue;
      final long trigger = nextTrigger(alarm, now);
      if (trigger > 0 && (next < 0 || trigger < next)) next = trigger;
      final long snooze = snoozeUntil(context, alarm.optString("id"));
      if (snooze > nowMillis && (next < 0 || snooze < next)) next = snooze;
    }
    return next;

  }

  // Returns the earliest end of a snooze after now in milliseconds since the epoch, or -1.
  static long nextSnooze(final Context context) {

    final long now = System.currentTimeMillis();
    long next = -1;
    final JSONArray list = alarms(context);
    for (int i = 0; i < list.length(); ++i) {
      final JSONObject alarm = list.optJSONObject(i);
      if (alarm == null) continue;
      final long snooze = snoozeUntil(context, alarm.optString("id"));
      if (snooze > now && (next < 0 || snooze < next)) next = snooze;
    }
    return next;

  }

  // Schedules every enabled alarm and snooze, and cancels the alarms that were removed or disabled.
  static void reschedule(final Context context) {

    synchronized (lock) {
      final AlarmManager manager = context.getSystemService(AlarmManager.class);
      final SharedPreferences preferences = preferences(context);
      final ZonedDateTime now = ZonedDateTime.now(ZoneId.systemDefault());
      final long nowMillis = now.toInstant().toEpochMilli();
      final Set<String> scheduled = new HashSet<>();
      final SharedPreferences.Editor editor = preferences.edit();
      final JSONArray list = alarms(context);
      for (int i = 0; i < list.length(); ++i) {
        final JSONObject alarm = list.optJSONObject(i);
        if (alarm == null) continue;
        final String id = alarm.optString("id");
        if (id.isEmpty()) continue;
        final long trigger = nextTrigger(alarm, now);
        if (trigger > 0 && schedule(context, manager, id, false, trigger)) scheduled.add(key(id, false));
        final long snooze = preferences.getLong(KEY_SNOOZE_PREFIX + id, 0);
        if (snooze > nowMillis) {
          if (schedule(context, manager, id, true, snooze)) scheduled.add(key(id, true));
        }
        else if (snooze > 0) {
          // A snooze that ended while the device was off is not rung late.
          editor.remove(KEY_SNOOZE_PREFIX + id);
        }
      }
      for (final String previous : preferences.getStringSet(KEY_SCHEDULED, new HashSet<>())) {
        if (scheduled.contains(previous)) continue;
        final int separator = previous.lastIndexOf('|');
        if (separator < 0) continue;
        final PendingIntent intent = PendingIntent.getBroadcast(context, 0, fireIntent(context, previous.substring(0, separator), previous.endsWith("|1")), PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_NO_CREATE);
        if (intent != null) {
          manager.cancel(intent);
          intent.cancel();
        }
      }
      editor.putStringSet(KEY_SCHEDULED, scheduled).commit();
      Log.i(TAG, "Scheduled " + scheduled.size() + " alarm(s)");
    }

  }

  private static String key(final String id, final boolean snooze) {
    return id + (snooze ? "|1" : "|0");
  }

  static Intent fireIntent(final Context context, final String id, final boolean snooze) {

    // The data makes the intents of different alarms distinct for PendingIntent.
    return new Intent(context, AlarmReceiver.class)
      .setAction(ACTION_FIRE)
      .setData(Uri.parse("strawberry-alarm://" + (snooze ? "snooze" : "alarm") + "/" + Uri.encode(id)))
      .putExtra(EXTRA_ID, id)
      .putExtra(EXTRA_SNOOZE, snooze);

  }

  private static boolean schedule(final Context context, final AlarmManager manager, final String id, final boolean snooze, final long trigger) {

    final PendingIntent operation = PendingIntent.getBroadcast(context, 0, fireIntent(context, id, snooze), PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    try {
      if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || manager.canScheduleExactAlarms()) {
        // An alarm clock alarm goes off exactly, also in Doze and battery saver, and shows the alarm icon in the status bar.
        final Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        final PendingIntent show = launch == null ? operation : PendingIntent.getActivity(context, 0, launch, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        manager.setAlarmClock(new AlarmManager.AlarmClockInfo(trigger, show), operation);
      }
      else {
        // setAlarmClock() needs the exact alarm permission too; without it AlarmReceiver can't start AlarmService and sounds through a notification instead.
        Log.w(TAG, "Exact alarms are not allowed, the alarm may ring late");
        manager.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, trigger, operation);
      }
      return true;
    }
    catch (SecurityException e) {
      Log.e(TAG, "Could not schedule an alarm", e);
      return false;
    }

  }

}
