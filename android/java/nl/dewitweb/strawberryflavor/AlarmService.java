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

import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.RingtoneManager;
import android.media.ToneGenerator;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

import org.json.JSONArray;
import org.json.JSONObject;

// Sounds an alarm: shows the full-screen alarm notification and plays the alarm's music, falling back to the default alarm sound.
// It plays copies of the songs that the Qt app saved for the alarm, or a radio stream, so it does not need the Qt app, the NAS or, except for radio, the network.
public class AlarmService extends Service {

  static final String ACTION_RING = "nl.dewitweb.strawberryflavor.ALARM_RING";
  static final String ACTION_SNOOZE = "nl.dewitweb.strawberryflavor.ALARM_SNOOZE";
  static final String ACTION_STOP = "nl.dewitweb.strawberryflavor.ALARM_STOP";
  // Set when AlarmReceiver already switched off the one-off alarm or ended the snooze.
  static final String EXTRA_CONSUMED = "consumed";

  // MediaSessionBridge uses 1 and BrowserAuthenticationService 2.
  private static final int NOTIFICATION_ID = 3;
  static final String CHANNEL_ID = "alarm";
  private static final long FADE_STEP_MSEC = 200;
  private static final long PREPARE_TIMEOUT_MSEC = 20000;
  private static final int CONNECT_TIMEOUT_MSEC = 10000;
  // Must match the prefix of NAS paths in nasfileengine.cpp.
  private static final String NAS_PREFIX = "/strawberry-nas/";

  // The alarm that is sounding, read by AlarmActivity on the main thread.
  private static String ringingId;
  private static String ringingName = "";
  // Why the default sound plays instead of the alarm's music, shown on the alarm screen, or empty.
  private static String status = "";
  private static Runnable finishedListener;
  private static Runnable statusListener;

  private final Handler handler = new Handler(Looper.getMainLooper());
  private AudioManager audioManager;
  private AudioFocusRequest focusRequest;
  private PowerManager.WakeLock wakeLock;
  private MediaPlayer player;
  private ToneGenerator toneGenerator;
  private final List<String> sources = new ArrayList<>();
  private int sourceIndex = 0;
  private int failures = 0;
  private String currentSource;
  private boolean radio = false;
  private boolean defaultSound = false;
  private String sourceName = "";
  // Whether the alarm had anything to play before falling back to the default sound.
  private boolean hadItems = false;
  private String notificationTitle = "";
  private int previousVolume = -1;
  private long fadeStart = 0;
  private long fadeMsec = 0;
  private int snoozeMinutes = 10;
  // Increased for every new player, so callbacks of a replaced player are ignored.
  private int generation = 0;

  // The service while it runs, for the answers of the Qt app that arrive through AlarmBridge.
  private static AlarmService instance;
  // Decides whether the Qt app or this service plays the alarm.
  private final AlarmHandover handover = new AlarmHandover();
  private String alarmJson = "";
  private final Runnable handoverGrace = () -> apply(handover.graceEnded(), handover.token());
  private final Runnable handoverTimeout = () -> {
    if (handover.state() == AlarmHandover.WAITING) Log.i(AlarmStore.TAG, "The app did not play the alarm in time");
    apply(handover.timeout(), handover.token());
  };

  private final Runnable fadeStep = this::fadeStep;
  private final Runnable autoStop = () -> {
    Log.i(AlarmStore.TAG, "Stopping the alarm automatically");
    finishRinging();
  };
  private final Runnable beep = this::beep;

  static boolean isRinging() {
    return ringingId != null;
  }

  static String ringingName() {
    return ringingName;
  }

  static void setFinishedListener(final Runnable listener) {
    finishedListener = listener;
  }

  static String status() {
    return status;
  }

  static void setStatusListener(final Runnable listener) {
    statusListener = listener;
  }

  // Returns true once per alarm while it waits for a Qt app that does not run yet; the alarm screen then starts the app.
  // Android blocks starting it from this service while no window of the app is visible, which the alarm screen is.
  // The app must stay in front while it starts: Qt creates its windows on the Android main thread only while the app is active.
  static boolean takeAppStart(final Context context) {

    final boolean locked = context.getSystemService(KeyguardManager.class).isDeviceLocked();
    if (instance == null || ringingId == null || !instance.handover.startAppFromScreen(AlarmBridge.isPlayerReady(), locked)) return false;
    Log.i(AlarmStore.TAG, "Starting the app from the alarm screen");
    return true;

  }

  static Intent appIntent(final Context context) {

    final Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
    if (launch != null) launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_USER_ACTION | Intent.FLAG_ACTIVITY_NO_ANIMATION);
    return launch;

  }

  // The Qt app's alarm player became ready, for example after this service started the app.
  static void onPlayerReady() {

    if (instance == null || ringingId == null) return;
    instance.apply(instance.handover.qtReady(), instance.handover.token());

  }

  static void onAlarmClaimed(final String token) {

    if (instance == null) {
      // The alarm was stopped meanwhile.
      AlarmBridge.stopAlarm(token);
      return;
    }
    final boolean waiting = instance.handover.state() == AlarmHandover.WAITING;
    final int actions = instance.handover.claimed(token);
    if (waiting && instance.handover.state() == AlarmHandover.QT) Log.i(AlarmStore.TAG, "The app plays the alarm");
    instance.apply(actions, token);

  }

  static void onAlarmFailed(final String token, final String reason) {

    if (instance == null) return;
    Log.i(AlarmStore.TAG, "The app does not play the alarm: " + reason);
    instance.apply(instance.handover.failed(token), token);

  }

  @Override
  public void onCreate() {

    super.onCreate();
    instance = this;

  }

  @Override
  public int onStartCommand(final Intent intent, final int flags, final int startId) {

    final String action = intent == null ? null : intent.getAction();
    if (ACTION_RING.equals(action)) {
      final String id = intent.getStringExtra(AlarmStore.EXTRA_ID);
      final JSONObject alarm = id == null ? null : AlarmStore.alarm(this, id);
      // The service must always enter the foreground after startForegroundService().
      startForeground(alarm == null ? ringingName : alarm.optString("name"));
      if (alarm == null) {
        Log.w(AlarmStore.TAG, "The alarm that fired no longer exists");
        // A stale alarm must not silence another alarm that is sounding.
        if (ringingId == null) stopService();
        return START_NOT_STICKY;
      }
      startRinging(alarm, intent.getBooleanExtra(AlarmStore.EXTRA_SNOOZE, false), intent.getBooleanExtra(EXTRA_CONSUMED, false));
      showRingingScreen();
    }
    else if (ACTION_SNOOZE.equals(action)) {
      if (ringingId != null) {
        Log.i(AlarmStore.TAG, "Snoozing the alarm for " + snoozeMinutes + " minute(s)");
        AlarmStore.setSnooze(this, ringingId, System.currentTimeMillis() + snoozeMinutes * 60000L);
      }
      finishRinging();
    }
    else if (ACTION_STOP.equals(action)) {
      Log.i(AlarmStore.TAG, "Stopping the alarm");
      finishRinging();
    }
    else if (ringingId == null) {
      stopService();
    }
    return START_NOT_STICKY;

  }

  @Override
  public IBinder onBind(final Intent intent) {
    return null;
  }

  @Override
  public void onDestroy() {

    if (ringingId != null) finishRinging();
    if (instance == this) instance = null;
    super.onDestroy();

  }

  // Creating an existing channel only updates its name; the user's settings stay.
  static void createChannel(final Context context) {

    final NotificationChannel channel = new NotificationChannel(CHANNEL_ID, AlarmStore.label(context, "channel", "Alarm clock"), NotificationManager.IMPORTANCE_HIGH);
    // The service plays the alarm itself.
    channel.setSound(null, null);
    channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
    context.getSystemService(NotificationManager.class).createNotificationChannel(channel);

  }

  private void startForeground(final String name) {

    createChannel(this);

    notificationTitle = name.isEmpty() ? AlarmStore.label(this, "alarm", "Alarm") : name;
    final Notification notification = buildNotification();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
    }
    else {
      startForeground(NOTIFICATION_ID, notification);
    }

  }

  private Notification buildNotification() {

    final Intent activity = new Intent(this, AlarmActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_USER_ACTION);
    final PendingIntent fullScreen = PendingIntent.getActivity(this, 0, activity, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    final String time = android.text.format.DateFormat.getTimeFormat(this).format(new java.util.Date());
    return new Notification.Builder(this, CHANNEL_ID)
      .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
      .setContentTitle(notificationTitle)
      .setContentText(status.isEmpty() ? time : status)
      .setCategory(Notification.CATEGORY_ALARM)
      .setVisibility(Notification.VISIBILITY_PUBLIC)
      .setOngoing(true)
      .setOnlyAlertOnce(true)
      .setShowWhen(false)
      .setContentIntent(fullScreen)
      .setFullScreenIntent(fullScreen, true)
      .addAction(new Notification.Action.Builder(null, AlarmStore.label(this, "snooze", "Snooze"), serviceIntent(ACTION_SNOOZE, 1)).build())
      .addAction(new Notification.Action.Builder(null, AlarmStore.label(this, "stop", "Stop"), serviceIntent(ACTION_STOP, 2)).build())
      .build();

  }

  // Android shows the full-screen notification as a heads-up while the phone is in use, and nothing at all without the notification permission.
  // Opening the alarm screen directly works at least while Strawberry is visible; otherwise Android ignores it.
  private void showRingingScreen() {

    if (ringingId == null) return;
    try {
      startActivity(new Intent(this, AlarmActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_USER_ACTION));
    }
    catch (RuntimeException e) {
      Log.w(AlarmStore.TAG, "Could not open the alarm screen", e);
    }

  }

  private void setStatus(final String text) {

    if (text.equals(status)) return;
    status = text;
    if (ringingId != null) {
      try {
        getSystemService(NotificationManager.class).notify(NOTIFICATION_ID, buildNotification());
      }
      catch (RuntimeException e) {
        Log.w(AlarmStore.TAG, "Could not update the alarm notification", e);
      }
    }
    final Runnable listener = statusListener;
    if (listener != null) listener.run();

  }

  private PendingIntent serviceIntent(final String action, final int requestCode) {
    return PendingIntent.getService(this, requestCode, new Intent(this, AlarmService.class).setAction(action), PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
  }

  private void startRinging(final JSONObject alarm, final boolean snoozed, final boolean consumed) {

    // A second alarm at the same time replaces the one that is sounding.
    if (ringingId != null) stopSound();

    ringingId = alarm.optString("id");
    ringingName = alarm.optString("name");
    snoozeMinutes = Math.max(1, alarm.optInt("snooze", 10));
    if (!consumed) AlarmStore.consumeAlarm(this, alarm, snoozed);

    final int autoStopMinutes = AlarmStore.autoStopMinutes(this);
    if (wakeLock == null) {
      wakeLock = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Strawberry:alarm");
      wakeLock.setReferenceCounted(false);
    }
    wakeLock.acquire((autoStopMinutes > 0 ? autoStopMinutes + 1 : 120) * 60000L);

    audioManager = getSystemService(AudioManager.class);
    final int maximum = audioManager.getStreamMaxVolume(AudioManager.STREAM_ALARM);
    final int volume = Math.max(1, Math.round(maximum * Math.max(0, Math.min(100, alarm.optInt("volume", 80))) / 100.0F));
    try {
      if (previousVolume < 0) previousVolume = audioManager.getStreamVolume(AudioManager.STREAM_ALARM);
      audioManager.setStreamVolume(AudioManager.STREAM_ALARM, volume, 0);
    }
    catch (SecurityException e) {
      Log.w(AlarmStore.TAG, "Could not set the alarm volume", e);
    }
    // Transient focus pauses Strawberry's own playback, which resumes after the alarm.
    // A second alarm keeps the focus of the first one instead of requesting it again.
    if (focusRequest == null) {
      focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT).setAudioAttributes(audioAttributes()).build();
      // An alarm sounds without the focus too, for example during a phone call.
      if (audioManager.requestAudioFocus(focusRequest) != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
        Log.w(AlarmStore.TAG, "Audio focus was not granted, ringing anyway");
      }
    }

    sources.clear();
    sourceIndex = 0;
    failures = 0;
    defaultSound = false;
    setStatus("");
    final String source = alarm.optString("source", "random");
    sourceName = source;
    radio = "radio".equals(source);
    if (radio) {
      final String url = alarm.optString("source_url");
      if (!url.isEmpty()) sources.add(url);
    }
    else if (!"sound".equals(source)) {
      // Local copies, or with copying switched off the original songs: local files, content:// or http(s) URLs.
      final JSONArray tracks = alarm.optJSONArray("tracks");
      for (int i = 0; tracks != null && i < tracks.length(); ++i) {
        final String track = tracks.optString(i);
        if (track.startsWith(NAS_PREFIX)) {
          // Only the Qt app's SMB client can read the NAS.
          Log.w(AlarmStore.TAG, "Skipping a NAS song, which can only ring from a copy");
        }
        else if (track.startsWith("/") ? new File(track).isFile() : isPlayableUrl(track)) {
          sources.add(track);
        }
      }
      if (sources.isEmpty() && "random".equals(source)) {
        // The alarm went off before its songs were chosen and copied, so play the songs copied for the other alarms.
        sources.addAll(AlarmFallback.copies(new File(getFilesDir(), "alarms"), sources));
        if (!sources.isEmpty()) Log.i(AlarmStore.TAG, "The alarm has no songs yet, ringing with songs copied for other alarms");
      }
      if ("random".equals(source)) Collections.shuffle(sources);
    }
    hadItems = !sources.isEmpty();
    Log.i(AlarmStore.TAG, "Ringing alarm with source " + source + " and " + sources.size() + " playable item(s)");

    fadeMsec = Math.max(0, alarm.optInt("fade_in", 30)) * 1000L;
    fadeStart = System.currentTimeMillis();
    alarmJson = alarm.toString();
    handler.removeCallbacks(handoverGrace);
    handler.removeCallbacks(handoverTimeout);
    final String previousToken = handover.token();
    final boolean ready = AlarmBridge.isPlayerReady();
    final int actions = handover.start(ringingId + ":" + SystemClock.elapsedRealtime(), !"sound".equals(source), ready);
    if (handover.state() == AlarmHandover.WAITING) Log.i(AlarmStore.TAG, ready ? "Asking the running app to play the alarm" : "Starting the app to play the alarm");
    if ((actions & AlarmHandover.STOP_QT) != 0) apply(AlarmHandover.STOP_QT, previousToken);
    apply(actions & ~AlarmHandover.STOP_QT, handover.token());
    handler.removeCallbacks(fadeStep);
    handler.post(fadeStep);
    handler.removeCallbacks(autoStop);
    if (autoStopMinutes > 0) handler.postDelayed(autoStop, autoStopMinutes * 60000L);

  }

  private static boolean isOnlineUrl(final String url) {

    final String lower = url.toLowerCase(Locale.ROOT);
    return lower.startsWith("http://") || lower.startsWith("https://");

  }

  private static boolean isPlayableUrl(final String url) {
    return isOnlineUrl(url) || url.toLowerCase(Locale.ROOT).startsWith("content://");
  }

  private static AudioAttributes audioAttributes() {
    return new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build();
  }

  // Carries out the actions of AlarmHandover; token is the alarm that a STOP_QT action stops.
  private void apply(final int actions, final String token) {

    if ((actions & AlarmHandover.STOP_QT) != 0 && token != null) AlarmBridge.stopAlarm(token);
    // START_APP needs no call here: the alarm screen starts the app once it is shown, see takeAppStart().
    if ((actions & AlarmHandover.SEND_REQUEST) != 0) {
      if (!AlarmBridge.startAlarm(handover.token(), alarmJson, fadeStart, fadeMsec)) {
        apply(handover.failed(handover.token()), handover.token());
      }
    }
    if ((actions & AlarmHandover.START_JAVA_SOUND) != 0 && ringingId != null) {
      Log.i(AlarmStore.TAG, "Playing the alarm without the app");
      playNext();
    }
    if ((actions & AlarmHandover.STOP_JAVA_SOUND) != 0) {
      stopSound();
      // The app plays the alarm's music, so a note about the default sound no longer applies.
      setStatus("");
    }
    if ((actions & AlarmHandover.SCHEDULE_GRACE) != 0) handler.postDelayed(handoverGrace, AlarmHandover.GRACE_MSEC);
    if ((actions & AlarmHandover.SCHEDULE_TIMEOUT) != 0) {
      handler.removeCallbacks(handoverTimeout);
      handler.postDelayed(handoverTimeout, AlarmHandover.TIMEOUT_MSEC);
    }
    if (handover.takeScreenAfterAppStart(AlarmBridge.isPlayerReady())) showRingingScreen();

  }

  private void playNext() {

    releasePlayer();
    if (defaultSound || sources.isEmpty() || failures >= sources.size()) {
      playDefaultSound();
      return;
    }
    final String source = sources.get(sourceIndex % sources.size());
    currentSource = source;
    ++sourceIndex;
    final int playerGeneration = ++generation;
    if (radio && isPlaylistUrl(source)) {
      // MediaPlayer can't play PLS and M3U playlists, so resolve them to their first stream.
      new Thread(() -> {
        final String stream = resolvePlaylist(source);
        handler.post(() -> {
          if (playerGeneration == generation && ringingId != null) play(stream, playerGeneration);
        });
      }).start();
      return;
    }
    play(source, playerGeneration);

  }

  private void play(final String source, final int playerGeneration) {

    if (source == null) {
      onPlayerFailed(playerGeneration);
      return;
    }
    player = new MediaPlayer();
    player.setAudioAttributes(audioAttributes());
    player.setWakeMode(this, PowerManager.PARTIAL_WAKE_LOCK);
    player.setOnPreparedListener(prepared -> {
      if (playerGeneration != generation) return;
      handler.removeCallbacks(prepareTimeout);
      failures = 0;
      applyVolume();
      prepared.start();
    });
    player.setOnCompletionListener(completed -> {
      if (playerGeneration != generation) return;
      // A radio stream that ends has lost its connection; continue with the default sound.
      if (radio) defaultSound = true;
      playNext();
    });
    player.setOnErrorListener((failed, what, extra) -> {
      Log.w(AlarmStore.TAG, "Could not play an alarm item: " + what + " " + extra);
      onPlayerFailed(playerGeneration);
      return true;
    });
    try {
      if (source.startsWith("/")) player.setDataSource(source);
      else player.setDataSource(this, Uri.parse(source));
      applyVolume();
      player.prepareAsync();
      handler.removeCallbacks(prepareTimeout);
      handler.postDelayed(prepareTimeout, PREPARE_TIMEOUT_MSEC);
    }
    catch (Exception e) {
      Log.w(AlarmStore.TAG, "Could not open an alarm item", e);
      onPlayerFailed(playerGeneration);
    }

  }

  private final Runnable prepareTimeout = () -> {
    Log.w(AlarmStore.TAG, "An alarm item did not start in time");
    onPlayerFailed(generation);
  };

  private void onPlayerFailed(final int playerGeneration) {

    if (playerGeneration != generation || ringingId == null) return;
    handler.removeCallbacks(prepareTimeout);
    ++failures;
    if (radio) defaultSound = true;
    else if (currentSource != null && isOnlineUrl(currentSource)) {
      // An online song that fails most likely means there is no network, so don't wait for the other online songs to fail too.
      final List<String> local = new ArrayList<>();
      for (final String item : sources) {
        if (!isOnlineUrl(item)) local.add(item);
      }
      sources.clear();
      sources.addAll(local);
      sourceIndex = 0;
      failures = 0;
    }
    // Post, so a failing player is not released from its own callback.
    handler.post(() -> {
      if (playerGeneration == generation && ringingId != null) playNext();
    });

  }

  private void playDefaultSound() {

    defaultSound = true;
    final int reason = AlarmFallback.reason(sourceName, hadItems);
    Log.i(AlarmStore.TAG, "Playing the default alarm sound" + (reason == AlarmFallback.REASON_NONE ? "" : reason == AlarmFallback.REASON_NO_SONG ? ": no song available" : ": the music could not be played"));
    if (reason == AlarmFallback.REASON_NO_SONG) setStatus(AlarmStore.label(this, "default_no_song", "Playing the default sound: no song available"));
    else if (reason == AlarmFallback.REASON_FAILED) setStatus(AlarmStore.label(this, "default_failed", "Playing the default sound: the music could not be played"));
    final int playerGeneration = ++generation;
    for (final int type : new int[] { RingtoneManager.TYPE_ALARM, RingtoneManager.TYPE_RINGTONE, RingtoneManager.TYPE_NOTIFICATION }) {
      final Uri uri = RingtoneManager.getDefaultUri(type);
      if (uri == null) continue;
      try {
        player = new MediaPlayer();
        player.setAudioAttributes(audioAttributes());
        player.setWakeMode(this, PowerManager.PARTIAL_WAKE_LOCK);
        player.setDataSource(this, uri);
        player.setLooping(true);
        player.setOnErrorListener((failed, what, extra) -> {
          if (playerGeneration == generation && ringingId != null) {
            releasePlayer();
            startBeeping();
          }
          return true;
        });
        player.prepare();
        applyVolume();
        player.start();
        return;
      }
      catch (Exception e) {
        Log.w(AlarmStore.TAG, "Could not play the default sound " + uri, e);
        releasePlayer();
      }
    }
    startBeeping();

  }

  // The last resort when no sound can be played: a generated beep.
  private void startBeeping() {

    if (toneGenerator != null) return;
    try {
      toneGenerator = new ToneGenerator(AudioManager.STREAM_ALARM, ToneGenerator.MAX_VOLUME);
      handler.post(beep);
    }
    catch (RuntimeException e) {
      Log.e(AlarmStore.TAG, "Could not create a tone generator", e);
    }

  }

  private void beep() {

    if (toneGenerator == null) return;
    toneGenerator.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 1000);
    handler.postDelayed(beep, 2000);

  }

  private float fadeVolume() {

    if (fadeMsec <= 0) return 1.0F;
    final float progress = Math.min(1.0F, (System.currentTimeMillis() - fadeStart) / (float) fadeMsec);
    // Loudness is perceived logarithmically, so a squared curve sounds like an even fade.
    return Math.max(0.01F, progress * progress);

  }

  private void applyVolume() {

    if (player == null) return;
    final float volume = fadeVolume();
    try {
      player.setVolume(volume, volume);
    }
    catch (IllegalStateException e) {
      // The player is not initialized yet; the prepared listener applies the volume.
    }

  }

  private void fadeStep() {

    applyVolume();
    if (ringingId != null && System.currentTimeMillis() - fadeStart < fadeMsec) handler.postDelayed(fadeStep, FADE_STEP_MSEC);

  }

  private static boolean isPlaylistUrl(final String url) {

    final String path = Uri.parse(url).getPath();
    if (path == null) return false;
    final String lower = path.toLowerCase(Locale.ROOT);
    return lower.endsWith(".pls") || lower.endsWith(".m3u");

  }

  private static String resolvePlaylist(final String url) {

    HttpURLConnection connection = null;
    try {
      connection = (HttpURLConnection) new URL(url).openConnection();
      connection.setConnectTimeout(CONNECT_TIMEOUT_MSEC);
      connection.setReadTimeout(CONNECT_TIMEOUT_MSEC);
      try (BufferedReader reader = new BufferedReader(new InputStreamReader(connection.getInputStream(), StandardCharsets.UTF_8))) {
        String line;
        while ((line = reader.readLine()) != null) {
          line = line.trim();
          final int equals = line.indexOf('=');
          if (line.toLowerCase(Locale.ROOT).startsWith("file") && equals > 0) line = line.substring(equals + 1).trim();
          if (line.startsWith("http://") || line.startsWith("https://")) return line;
        }
      }
    }
    catch (Exception e) {
      Log.w(AlarmStore.TAG, "Could not read the radio playlist", e);
    }
    finally {
      if (connection != null) connection.disconnect();
    }
    return null;

  }

  private void releasePlayer() {

    handler.removeCallbacks(prepareTimeout);
    if (player == null) return;
    try {
      player.release();
    }
    catch (RuntimeException e) {
      Log.w(AlarmStore.TAG, "Could not release the alarm player", e);
    }
    player = null;

  }

  private void stopSound() {

    ++generation;
    releasePlayer();
    handler.removeCallbacks(beep);
    if (toneGenerator != null) {
      toneGenerator.release();
      toneGenerator = null;
    }

  }

  private void finishRinging() {

    handler.removeCallbacks(fadeStep);
    handler.removeCallbacks(autoStop);
    handler.removeCallbacks(handoverGrace);
    handler.removeCallbacks(handoverTimeout);
    final String token = handover.token();
    apply(handover.finish(), token);
    stopSound();
    if (audioManager != null) {
      if (focusRequest != null) audioManager.abandonAudioFocusRequest(focusRequest);
      if (previousVolume >= 0) {
        try {
          audioManager.setStreamVolume(AudioManager.STREAM_ALARM, previousVolume, 0);
        }
        catch (SecurityException e) {
          Log.w(AlarmStore.TAG, "Could not restore the alarm volume", e);
        }
      }
    }
    focusRequest = null;
    previousVolume = -1;
    ringingId = null;
    ringingName = "";
    status = "";
    if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
    final Runnable listener = finishedListener;
    if (listener != null) listener.run();
    stopService();

  }

  private void stopService() {

    stopForeground(STOP_FOREGROUND_REMOVE);
    stopSelf();

  }

}
