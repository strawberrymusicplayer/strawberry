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

import android.app.Activity;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONObject;

// The full-screen alarm screen, also shown over the lock screen, with the clock, the alarm name and big Snooze and Stop buttons.
// The buttons are measured first at the bottom, so they stay on screen in landscape and with large fonts; the clock shrinks instead.
// Opened from AlarmReceiver's fallback notification, it starts sounding the alarm itself, which a visible activity is allowed to do.
public class AlarmActivity extends Activity {

  private ClockView clock;
  private TextView status;
  // The alarm to start sounding when this screen is shown, set when opened from the fallback notification.
  private String startId;

  @Override
  protected void onCreate(final Bundle savedInstanceState) {

    super.onCreate(savedInstanceState);
    setShowWhenLocked(true);
    setTurnScreenOn(true);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    // Both the Java sound and the app's player use the alarm stream; without this, the volume keys change the media volume.
    // The keys are not handled here, so they never snooze or stop the alarm.
    setVolumeControlStream(AudioManager.STREAM_ALARM);

    clock = new ClockView(this);
    clock.detail.setTextColor(ClockView.TIME_COLOR);
    clock.detail.setTextSize(TypedValue.COMPLEX_UNIT_SP, 32);
    ClockView.hide(clock.bottom);
    // The outer layout keeps the clock and the buttons clear of the status and navigation bars.
    clock.root.setFitsSystemWindows(false);
    clock.root.setPadding(0, 0, 0, 0);

    final LinearLayout root = new LinearLayout(this);
    root.setOrientation(LinearLayout.VERTICAL);
    root.setBackgroundColor(ClockView.BACKGROUND);
    root.setFitsSystemWindows(true);
    final int padding = ClockView.dp(this, 16);
    root.setPadding(padding, padding, padding, padding);
    root.addView(clock.root, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0F));

    status = new TextView(this);
    status.setTextColor(ClockView.ACCENT_COLOR);
    status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
    status.setGravity(Gravity.CENTER);
    status.setPadding(0, padding / 2, 0, 0);
    root.addView(status, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

    final LinearLayout buttons = new LinearLayout(this);
    buttons.setOrientation(LinearLayout.HORIZONTAL);
    final Button snooze = ClockView.createButton(this, AlarmStore.label(this, "snooze", "Snooze"), 0xFF424242, ClockView.TIME_COLOR);
    snooze.setOnClickListener(view -> send(AlarmService.ACTION_SNOOZE));
    buttons.addView(snooze, ClockView.buttonLayout(0));
    final Button stop = ClockView.createButton(this, AlarmStore.label(this, "stop", "Stop"), ClockView.ACCENT_COLOR, ClockView.BACKGROUND);
    stop.setOnClickListener(view -> send(AlarmService.ACTION_STOP));
    buttons.addView(stop, ClockView.buttonLayout(padding));
    final LinearLayout.LayoutParams buttonsLayout = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    buttonsLayout.topMargin = padding;
    root.addView(buttons, buttonsLayout);

    setContentView(root);
    AlarmService.setStatusListener(this::showStatus);
    showStatus();
    AlarmService.setFinishedListener(this::finish);
    handleIntent(getIntent());

  }

  @Override
  protected void onNewIntent(final Intent intent) {

    super.onNewIntent(intent);
    setIntent(intent);
    handleIntent(intent);

  }

  private void showStatus() {

    final String text = AlarmService.status();
    status.setText(text);
    status.setVisibility(text.isEmpty() ? android.view.View.GONE : android.view.View.VISIBLE);

  }

  private void handleIntent(final Intent intent) {

    final String id = intent.getStringExtra(AlarmStore.EXTRA_ID);
    startId = id != null && !AlarmService.isRinging() ? id : null;
    String name = AlarmService.ringingName();
    if (startId != null) {
      final JSONObject alarm = AlarmStore.alarm(this, startId);
      name = alarm == null ? "" : alarm.optString("name");
    }
    clock.detail.setText(name.isEmpty() ? AlarmStore.label(this, "alarm", "Alarm") : name);

  }

  @Override
  protected void onResume() {

    super.onResume();
    if (startId != null) {
      // AlarmReceiver already switched off a one-off alarm or ended the snooze, so the service only plays it.
      final Intent ring = new Intent(this, AlarmService.class)
        .setAction(AlarmService.ACTION_RING)
        .putExtra(AlarmStore.EXTRA_ID, startId)
        .putExtra(AlarmService.EXTRA_CONSUMED, true);
      startId = null;
      try {
        startForegroundService(ring);
        AlarmReceiver.cancelFallback(this);
      }
      catch (RuntimeException e) {
        Log.e(AlarmStore.TAG, "Could not start ringing", e);
      }
      return;
    }
    // The alarm may have been stopped from the notification while this screen was away.
    if (!AlarmService.isRinging()) {
      finish();
      return;
    }
    if (AlarmService.takeAppStart()) startApp();

  }

  // Starts the Qt app so it can play the alarm; AlarmService brings this screen back once the app is ready.
  private void startApp() {

    final Intent launch = AlarmService.appIntent(this);
    if (launch == null) return;
    try {
      startActivity(launch);
    }
    catch (RuntimeException e) {
      Log.w(AlarmStore.TAG, "Could not start the app", e);
    }

  }

  @Override
  protected void onDestroy() {

    AlarmService.setFinishedListener(null);
    AlarmService.setStatusListener(null);
    super.onDestroy();

  }

  // Back must not silence the alarm by accident; Snooze or Stop does.
  @Override
  @SuppressWarnings("deprecation")
  public void onBackPressed() {}

  private void send(final String action) {

    startService(new Intent(this, AlarmService.class).setAction(action));
    AlarmReceiver.cancelFallback(this);
    finish();

  }

}
