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
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

// Publishes the current song and playback state from AndroidMediaSession (C++) to a MediaSession and a media notification, and runs PlaybackService while a song is playing or paused.
// The public methods are called from the Qt thread; all state is only accessed on the Android main thread.
public final class MediaSessionBridge {

  public static final int STATE_STOPPED = 0;
  public static final int STATE_PLAYING = 1;
  public static final int STATE_PAUSED = 2;

  static final int NOTIFICATION_ID = 1;

  private static final String TAG = "Strawberry";
  private static final String CHANNEL_ID = "playback";
  private static final int MAX_COVER_SIZE = 512;

  private static final Handler handler = new Handler(Looper.getMainLooper());

  private static Context context;
  private static MediaSession session;
  private static PendingIntent contentIntent;
  private static int state = STATE_STOPPED;
  private static boolean serviceStarted = false;
  private static String title = "";
  private static String artist = "";
  private static String album = "";
  private static Bitmap cover;

  private MediaSessionBridge() {}

  public static void init(final Context appContext, final String channelName) {

    final Context applicationContext = appContext.getApplicationContext();
    handler.post(() -> {
      context = applicationContext;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        final NotificationChannel channel = new NotificationChannel(CHANNEL_ID, channelName, NotificationManager.IMPORTANCE_LOW);
        channel.setShowBadge(false);
        context.getSystemService(NotificationManager.class).createNotificationChannel(channel);
      }
      final Intent launchIntent = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
      contentIntent = launchIntent == null ? null : PendingIntent.getActivity(context, 0, launchIntent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
      session = new MediaSession(context, TAG);
      session.setSessionActivity(contentIntent);
    });

  }

  public static void setMetadata(final String newTitle, final String newArtist, final String newAlbum, final long durationMsec, final String coverPath) {

    // Decode the cover on the calling thread, to keep the main thread responsive.
    final Bitmap newCover = decodeCover(coverPath);
    handler.post(() -> {
      if (session == null) return;
      title = newTitle;
      artist = newArtist;
      album = newAlbum;
      cover = newCover;
      final MediaMetadata.Builder builder = new MediaMetadata.Builder()
        .putString(MediaMetadata.METADATA_KEY_TITLE, title)
        .putString(MediaMetadata.METADATA_KEY_ARTIST, artist)
        .putString(MediaMetadata.METADATA_KEY_ALBUM, album);
      if (durationMsec > 0) {
        builder.putLong(MediaMetadata.METADATA_KEY_DURATION, durationMsec);
      }
      if (cover != null) {
        builder.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, cover);
      }
      session.setMetadata(builder.build());
      updateNotification();
    });

  }

  public static void setPlaybackState(final int newState, final long positionMsec) {

    handler.post(() -> {
      if (session == null) return;
      state = newState;
      final int sessionState = state == STATE_PLAYING ? PlaybackState.STATE_PLAYING : state == STATE_PAUSED ? PlaybackState.STATE_PAUSED : PlaybackState.STATE_STOPPED;
      session.setPlaybackState(new PlaybackState.Builder()
        .setState(sessionState, positionMsec, state == STATE_PLAYING ? 1.0F : 0.0F, SystemClock.elapsedRealtime())
        .build());
      session.setActive(state != STATE_STOPPED);
      if (state == STATE_STOPPED) {
        stopPlaybackService();
      }
      else if (serviceStarted) {
        updateNotification();
      }
      else {
        startPlaybackService();
      }
    });

  }

  static Notification buildNotification(final Context notificationContext) {

    final Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? new Notification.Builder(notificationContext, CHANNEL_ID) : new Notification.Builder(notificationContext);
    builder.setSmallIcon(state == STATE_PLAYING ? android.R.drawable.ic_media_play : android.R.drawable.ic_media_pause)
      .setContentTitle(title)
      .setContentText(artist)
      .setSubText(album)
      .setLargeIcon(cover)
      .setContentIntent(contentIntent)
      .setShowWhen(false)
      .setOngoing(true)
      .setVisibility(Notification.VISIBILITY_PUBLIC)
      .setStyle(new Notification.MediaStyle().setMediaSession(session.getSessionToken()));
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
    }
    return builder.build();

  }

  private static void startPlaybackService() {

    final Intent intent = new Intent(context, PlaybackService.class);
    try {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        context.startForegroundService(intent);
      }
      else {
        context.startService(intent);
      }
      serviceStarted = true;
    }
    catch (RuntimeException e) {
      // Android 12 and later do not allow starting a foreground service while the app is in the background.
      Log.e(TAG, "Could not start the playback service", e);
    }

  }

  private static void stopPlaybackService() {

    if (!serviceStarted) return;
    context.stopService(new Intent(context, PlaybackService.class));
    serviceStarted = false;

  }

  private static void updateNotification() {

    if (!serviceStarted) return;
    context.getSystemService(NotificationManager.class).notify(NOTIFICATION_ID, buildNotification(context));

  }

  private static Bitmap decodeCover(final String path) {

    if (path == null || path.isEmpty()) return null;

    final BitmapFactory.Options options = new BitmapFactory.Options();
    options.inJustDecodeBounds = true;
    BitmapFactory.decodeFile(path, options);
    if (options.outWidth <= 0 || options.outHeight <= 0) return null;

    options.inJustDecodeBounds = false;
    options.inSampleSize = 1;
    while (Math.max(options.outWidth, options.outHeight) / (options.inSampleSize * 2) >= MAX_COVER_SIZE) {
      options.inSampleSize *= 2;
    }
    final Bitmap bitmap = BitmapFactory.decodeFile(path, options);
    if (bitmap == null) return null;

    // Media session and notification bitmaps are sent to the system in one binder transaction, which is limited in size.
    final int size = Math.max(bitmap.getWidth(), bitmap.getHeight());
    if (size <= MAX_COVER_SIZE) return bitmap;
    final float scale = (float) MAX_COVER_SIZE / size;
    return Bitmap.createScaledBitmap(bitmap, Math.round(bitmap.getWidth() * scale), Math.round(bitmap.getHeight() * scale), true);

  }

}
