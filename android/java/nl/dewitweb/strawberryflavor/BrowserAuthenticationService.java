// SPDX-License-Identifier: GPL-3.0-or-later

package nl.dewitweb.strawberryflavor;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

public class BrowserAuthenticationService extends Service {
  private static final String CHANNEL = "browser-authentication";
  private static final int NOTIFICATION_ID = 2;
  private static final long TIMEOUT_MS = 120000;
  private final Handler handler = new Handler(Looper.getMainLooper());
  private final Runnable timeout = () -> finish(2);
  private long requestId;

  private static native void nativeFinished(long requestId, int reason);

  public static boolean start(Context context, long id, String channel, String title, String text) {
    Intent intent = new Intent(context, BrowserAuthenticationService.class);
    intent.putExtra("requestId", id);
    intent.putExtra("channel", channel);
    intent.putExtra("title", title);
    intent.putExtra("text", text);
    try {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        return context.startForegroundService(intent) != null;
      }
      return context.startService(intent) != null;
    }
    catch (IllegalStateException | SecurityException error) {
      Log.e("StrawberryAuth", "Could not start authentication service", error);
      return false;
    }
  }

  public static void stop(Context context) {
    context.stopService(new Intent(context, BrowserAuthenticationService.class));
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    if (intent == null) {
      stopSelf(startId);
      return START_NOT_STICKY;
    }
    requestId = intent.getLongExtra("requestId", 0);
    NotificationManager manager = getSystemService(NotificationManager.class);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      manager.createNotificationChannel(new NotificationChannel(CHANNEL, intent.getStringExtra("channel"), NotificationManager.IMPORTANCE_LOW));
    }
    Intent returnIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());
    if (returnIntent == null) {
      finish(3);
      return START_NOT_STICKY;
    }
    returnIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
    PendingIntent pending = PendingIntent.getActivity(this, 1, returnIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
        ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
    Notification notification = builder
        .setSmallIcon(android.R.drawable.ic_lock_lock)
        .setContentTitle(intent.getStringExtra("title"))
        .setContentText(intent.getStringExtra("text"))
        .setContentIntent(pending)
        .setOngoing(true)
        .build();
    try {
      if (Build.VERSION.SDK_INT >= 34) {
        startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SHORT_SERVICE);
      }
      else {
        startForeground(NOTIFICATION_ID, notification);
      }
    }
    catch (IllegalArgumentException | IllegalStateException | SecurityException error) {
      Log.e("StrawberryAuth", "Could not enter foreground authentication", error);
      finish(3);
      return START_NOT_STICKY;
    }
    handler.removeCallbacks(timeout);
    handler.postDelayed(timeout, TIMEOUT_MS);
    return START_NOT_STICKY;
  }

  private void finish(int reason) {
    handler.removeCallbacks(timeout);
    stopForeground(STOP_FOREGROUND_REMOVE);
    stopSelf();
    nativeFinished(requestId, reason);
  }

  @Override
  public void onTimeout(int startId) {
    finish(2);
  }

  @Override
  public void onDestroy() {
    handler.removeCallbacks(timeout);
    super.onDestroy();
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }
}
