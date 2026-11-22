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

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.RingtoneManager;
import android.net.Uri;
import android.util.Log;

import org.json.JSONObject;

// Receives the scheduled alarms from AlarmManager and starts ringing; not exported, only AlarmStore's pending intents reach it.
// Without the exact alarm permission (Android 12 and 12L), an alarm does not allow starting a foreground service from the background.
// The alarm then rings through a full-screen notification with the default alarm sound; opening it starts the alarm's music in AlarmActivity.
public final class AlarmReceiver extends BroadcastReceiver {

  static final String ACTION_FALLBACK_SNOOZE = "nl.dewitweb.strawberryflavor.ALARM_FALLBACK_SNOOZE";
  static final String ACTION_FALLBACK_STOP = "nl.dewitweb.strawberryflavor.ALARM_FALLBACK_STOP";
  // MediaSessionBridge uses 1, BrowserAuthenticationService 2 and AlarmService 3.
  static final int FALLBACK_NOTIFICATION_ID = 4;
  private static final String FALLBACK_CHANNEL_ID = "alarm_fallback";

  @Override
  public void onReceive(final Context context, final Intent intent) {

    final String action = intent.getAction();
    final String id = intent.getStringExtra(AlarmStore.EXTRA_ID);
    if (id == null) return;
    if (ACTION_FALLBACK_SNOOZE.equals(action)) {
      final JSONObject alarm = AlarmStore.alarm(context, id);
      if (alarm != null) {
        AlarmStore.setSnooze(context, id, System.currentTimeMillis() + Math.max(1, alarm.optInt("snooze", 10)) * 60000L);
        AlarmStore.reschedule(context);
      }
      cancelFallback(context);
      return;
    }
    if (ACTION_FALLBACK_STOP.equals(action)) {
      cancelFallback(context);
      return;
    }
    if (!AlarmStore.ACTION_FIRE.equals(action)) return;
    final boolean snoozed = intent.getBooleanExtra(AlarmStore.EXTRA_SNOOZE, false);
    Log.i(AlarmStore.TAG, "Alarm fired" + (snoozed ? " after snoozing" : ""));
    // An alarm clock alarm allows starting a foreground service from the background.
    final Intent ring = new Intent(context, AlarmService.class)
      .setAction(AlarmService.ACTION_RING)
      .putExtra(AlarmStore.EXTRA_ID, id)
      .putExtra(AlarmStore.EXTRA_SNOOZE, snoozed);
    try {
      context.startForegroundService(ring);
    }
    catch (RuntimeException e) {
      Log.w(AlarmStore.TAG, "Could not start ringing, ringing through a notification", e);
      showFallback(context, id, snoozed);
    }

  }

  static void cancelFallback(final Context context) {
    context.getSystemService(NotificationManager.class).cancel(FALLBACK_NOTIFICATION_ID);
  }

  private static void showFallback(final Context context, final String id, final boolean snoozed) {

    final JSONObject alarm = AlarmStore.alarm(context, id);
    if (alarm == null) return;
    AlarmStore.consumeAlarm(context, alarm, snoozed);

    final NotificationManager notifications = context.getSystemService(NotificationManager.class);
    final NotificationChannel channel = new NotificationChannel(FALLBACK_CHANNEL_ID, AlarmStore.label(context, "channel", "Alarm clock"), NotificationManager.IMPORTANCE_HIGH);
    channel.setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM), new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build());
    channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
    notifications.createNotificationChannel(channel);

    final String name = alarm.optString("name");
    final Intent activity = new Intent(context, AlarmActivity.class)
      .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_USER_ACTION)
      .putExtra(AlarmStore.EXTRA_ID, id);
    final PendingIntent fullScreen = PendingIntent.getActivity(context, 3, activity, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    final int autoStopMinutes = AlarmStore.autoStopMinutes(context);
    final Notification.Builder builder = new Notification.Builder(context, FALLBACK_CHANNEL_ID)
      .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
      .setContentTitle(name.isEmpty() ? AlarmStore.label(context, "alarm", "Alarm") : name)
      .setContentText(android.text.format.DateFormat.getTimeFormat(context).format(new java.util.Date()))
      .setCategory(Notification.CATEGORY_ALARM)
      .setVisibility(Notification.VISIBILITY_PUBLIC)
      .setOngoing(true)
      .setShowWhen(false)
      .setContentIntent(fullScreen)
      .setFullScreenIntent(fullScreen, true)
      .addAction(new Notification.Action.Builder(null, AlarmStore.label(context, "snooze", "Snooze"), receiverIntent(context, ACTION_FALLBACK_SNOOZE, id)).build())
      .addAction(new Notification.Action.Builder(null, AlarmStore.label(context, "stop", "Stop"), receiverIntent(context, ACTION_FALLBACK_STOP, id)).build());
    if (autoStopMinutes > 0) builder.setTimeoutAfter(autoStopMinutes * 60000L);
    final Notification notification = builder.build();
    // The sound repeats until the notification is answered or times out.
    notification.flags |= Notification.FLAG_INSISTENT;
    notifications.notify(FALLBACK_NOTIFICATION_ID, notification);

  }

  private static PendingIntent receiverIntent(final Context context, final String action, final String id) {

    final Intent intent = new Intent(context, AlarmReceiver.class)
      .setAction(action)
      .setData(Uri.parse("strawberry-alarm://" + Uri.encode(action) + "/" + Uri.encode(id)))
      .putExtra(AlarmStore.EXTRA_ID, id);
    return PendingIntent.getBroadcast(context, 0, intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

  }

}
