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
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

// Publishes the current song and playback state from AndroidMediaSession (C++) to a MediaSession and a media notification, and runs PlaybackService while a song is playing or paused.
// While playing, it holds audio focus and pauses when focus is lost or the audio output becomes noisy (headphones unplugged).
// The public methods are called from the Qt thread; all state is only accessed on the Android main thread.
public final class MediaSessionBridge {

  public static final int STATE_STOPPED = 0;
  public static final int STATE_PLAYING = 1;
  public static final int STATE_PAUSED = 2;

  static final int NOTIFICATION_ID = 1;

  private static final String TAG = "Strawberry";
  private static final String CHANNEL_ID = "playback";
  private static final int MAX_COVER_SIZE = 512;
  static final String ACTION_PLAY = "nl.dewitweb.strawberryflavor.PLAY";
  static final String ACTION_PAUSE = "nl.dewitweb.strawberryflavor.PAUSE";
  static final String ACTION_PREVIOUS = "nl.dewitweb.strawberryflavor.PREVIOUS";
  static final String ACTION_NEXT = "nl.dewitweb.strawberryflavor.NEXT";

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

  private static AudioManager audioManager;
  private static AudioFocusRequest audioFocusRequest;
  // True while a focus request is granted or delayed; a delayed request is granted later through onAudioFocusChange().
  private static boolean audioFocusRequested = false;
  // True while the app has audio focus, so not during a transient loss or before a delayed request is granted.
  private static boolean audioFocusHeld = false;
  // True while playback is paused because of a transient focus loss (or a delayed request), so it resumes when focus is gained.
  private static boolean resumeOnFocusGain = false;
  private static boolean noisyReceiverRegistered = false;

  private static final AudioManager.OnAudioFocusChangeListener audioFocusListener = MediaSessionBridge::onAudioFocusChange;

  private static final BroadcastReceiver noisyReceiver = new BroadcastReceiver() {
    @Override
    public void onReceive(final Context receiverContext, final Intent intent) {
      if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction()) && state == STATE_PLAYING) {
        resumeOnFocusGain = false;
        nativePause();
      }
    }
  };

  private static native void nativePlay();
  private static native void nativePause();
  private static native void nativeNext();
  private static native void nativePrevious();
  private static native void nativeSeekTo(long positionMsec);
  private static native void nativeStop();

  private MediaSessionBridge() {}

  public static void init(final Context appContext, final String channelName) {

    final Context applicationContext = appContext.getApplicationContext();
    handler.post(() -> {
      context = applicationContext;
      audioManager = context.getSystemService(AudioManager.class);
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        // Without setWillPauseWhenDucked(), Android ducks the volume itself when another app requests transient focus that allows ducking.
        audioFocusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
          .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
          .setAcceptsDelayedFocusGain(true)
          .setOnAudioFocusChangeListener(audioFocusListener, handler)
          .build();
        final NotificationChannel channel = new NotificationChannel(CHANNEL_ID, channelName, NotificationManager.IMPORTANCE_LOW);
        channel.setShowBadge(false);
        context.getSystemService(NotificationManager.class).createNotificationChannel(channel);
      }
      final Intent launchIntent = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
      contentIntent = launchIntent == null ? null : PendingIntent.getActivity(context, 0, launchIntent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
      session = new MediaSession(context, TAG);
      session.setSessionActivity(contentIntent);
      session.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS | MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS);
      session.setCallback(new MediaSession.Callback() {
        @Override
        public void onPlay() {
          nativePlay();
        }

        @Override
        public void onPause() {
          // An explicit pause, also while already paused for a focus loss, cancels resuming when focus returns.
          resumeOnFocusGain = false;
          if (state != STATE_PLAYING) abandonAudioFocus();
          nativePause();
        }

        @Override
        public void onSkipToNext() {
          nativeNext();
        }

        @Override
        public void onSkipToPrevious() {
          nativePrevious();
        }

        @Override
        public void onSeekTo(final long positionMsec) {
          nativeSeekTo(positionMsec);
        }

        @Override
        public void onStop() {
          resumeOnFocusGain = false;
          nativeStop();
        }
      }, handler);
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
      final int previousState = state;
      state = newState;
      updateAudioFocus(previousState);
      final int sessionState = state == STATE_PLAYING ? PlaybackState.STATE_PLAYING : state == STATE_PAUSED ? PlaybackState.STATE_PAUSED : PlaybackState.STATE_STOPPED;
      session.setPlaybackState(new PlaybackState.Builder()
        .setState(sessionState, positionMsec, state == STATE_PLAYING ? 1.0F : 0.0F, SystemClock.elapsedRealtime())
        .setActions(PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE | PlaybackState.ACTION_PLAY_PAUSE |
          PlaybackState.ACTION_SKIP_TO_NEXT | PlaybackState.ACTION_SKIP_TO_PREVIOUS | PlaybackState.ACTION_SEEK_TO | PlaybackState.ACTION_STOP)
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
      .addAction(android.R.drawable.ic_media_previous, "Previous", actionIntent(notificationContext, ACTION_PREVIOUS, 0))
      .addAction(state == STATE_PLAYING ? android.R.drawable.ic_media_pause : android.R.drawable.ic_media_play,
        state == STATE_PLAYING ? "Pause" : "Play",
        actionIntent(notificationContext, state == STATE_PLAYING ? ACTION_PAUSE : ACTION_PLAY, 1))
      .addAction(android.R.drawable.ic_media_next, "Next", actionIntent(notificationContext, ACTION_NEXT, 2))
      .setStyle(new Notification.MediaStyle().setMediaSession(session.getSessionToken()).setShowActionsInCompactView(0, 1, 2));
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
    }
    return builder.build();

  }

  private static PendingIntent actionIntent(final Context actionContext, final String action, final int requestCode) {

    final Intent intent = new Intent(actionContext, MediaActionReceiver.class).setAction(action);
    return PendingIntent.getBroadcast(actionContext, requestCode, intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

  }

  static void handleAction(final String action) {

    handler.post(() -> {
      if (session == null) return;
      final MediaController.TransportControls controls = session.getController().getTransportControls();
      if (ACTION_PLAY.equals(action)) controls.play();
      else if (ACTION_PAUSE.equals(action)) controls.pause();
      else if (ACTION_PREVIOUS.equals(action)) controls.skipToPrevious();
      else if (ACTION_NEXT.equals(action)) controls.skipToNext();
    });

  }

  private static void updateAudioFocus(final int previousState) {

    if (state == STATE_PLAYING) {
      registerNoisyReceiver();
      // Only a new start of playback requests focus and ends a pause for focus loss.
      // Repeated playing states (seeks, song changes, or a pause still on its way) must not take focus back from another app.
      if (previousState != STATE_PLAYING) {
        resumeOnFocusGain = false;
        if (!audioFocusHeld) {
          requestAudioFocus();
        }
      }
      return;
    }

    unregisterNoisyReceiver();
    // Keep the request while paused for a focus loss, so playback resumes when focus returns.
    if (state == STATE_PAUSED && resumeOnFocusGain) {
      // Focus may already have returned before this pause arrived.
      if (audioFocusHeld) {
        resumeOnFocusGain = false;
        nativePlay();
      }
      return;
    }
    resumeOnFocusGain = false;
    abandonAudioFocus();

  }

  @SuppressWarnings("deprecation")
  private static void requestAudioFocus() {

    final int result = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? audioManager.requestAudioFocus(audioFocusRequest) : audioManager.requestAudioFocus(audioFocusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
    switch (result) {
      case AudioManager.AUDIOFOCUS_REQUEST_GRANTED:
        audioFocusRequested = true;
        audioFocusHeld = true;
        break;
      case AudioManager.AUDIOFOCUS_REQUEST_DELAYED:
        // For example during a phone call: pause now, and play when the delayed request is granted.
        audioFocusRequested = true;
        resumeOnFocusGain = true;
        nativePause();
        break;
      default:
        Log.w(TAG, "Audio focus request failed, pausing playback");
        nativePause();
        break;
    }

  }

  @SuppressWarnings("deprecation")
  private static void abandonAudioFocus() {

    if (!audioFocusRequested) return;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      audioManager.abandonAudioFocusRequest(audioFocusRequest);
    }
    else {
      audioManager.abandonAudioFocus(audioFocusListener);
    }
    audioFocusRequested = false;
    audioFocusHeld = false;

  }

  private static void onAudioFocusChange(final int focusChange) {

    Log.d(TAG, "Audio focus change: " + focusChange);
    switch (focusChange) {
      case AudioManager.AUDIOFOCUS_GAIN:
        audioFocusHeld = true;
        if (resumeOnFocusGain && state == STATE_PAUSED) {
          resumeOnFocusGain = false;
          nativePlay();
        }
        break;
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
        // Android 8 and later duck the volume automatically; earlier versions leave it to the app, so pause instead.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) break;
        // fall through
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
        audioFocusHeld = false;
        if (state == STATE_PLAYING) {
          resumeOnFocusGain = true;
          nativePause();
        }
        break;
      case AudioManager.AUDIOFOCUS_LOSS:
        // Another app started playing; do not resume automatically.
        resumeOnFocusGain = false;
        abandonAudioFocus();
        if (state == STATE_PLAYING) nativePause();
        break;
      default:
        break;
    }

  }

  private static void registerNoisyReceiver() {

    if (noisyReceiverRegistered) return;
    final IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
      context.registerReceiver(noisyReceiver, filter, null, handler, Context.RECEIVER_NOT_EXPORTED);
    }
    else {
      context.registerReceiver(noisyReceiver, filter, null, handler);
    }
    noisyReceiverRegistered = true;

  }

  private static void unregisterNoisyReceiver() {

    if (!noisyReceiverRegistered) return;
    context.unregisterReceiver(noisyReceiver);
    noisyReceiverRegistered = false;

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
