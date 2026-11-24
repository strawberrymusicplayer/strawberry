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
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

// The Tools > Clock > Show clock screen: a fullscreen digital or analog clock without system bars, at the brightness chosen in Clock settings.
// It keeps the screen on while it is open and shows the next alarm below the clock; the red X and Back return to Tools > Clock.
public class ClockActivity extends Activity {

  private final Handler handler = new Handler(Looper.getMainLooper());
  private ClockView clock;
  private LinearLayout brightnessControls;
  private SeekBar brightnessSlider;
  private TextView brightnessValue;
  private final Runnable refresh = this::refresh;

  @Override
  protected void onCreate(final Bundle savedInstanceState) {

    super.onCreate(savedInstanceState);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
      getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    }

    clock = new ClockView(this);
    // Fullscreen: the system bars are hidden, so the clock does not leave room for them.
    clock.root.setFitsSystemWindows(false);
    // Without alarm buttons the time takes all free space.
    clock.bottom.setVisibility(View.GONE);

    final FrameLayout frame = new FrameLayout(this);
    frame.addView(clock.root, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    final LinearLayout controls = new LinearLayout(this);
    controls.setOrientation(LinearLayout.VERTICAL);
    final LinearLayout buttons = new LinearLayout(this);
    buttons.setGravity(Gravity.END);
    controls.addView(buttons, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    final int size = ClockView.dp(this, 56);
    final int padding = ClockView.dp(this, 8);

    final ImageButton brightness = new ImageButton(this);
    brightness.setImageDrawable(new BrightnessIcon());
    brightness.setBackground(null);
    brightness.setScaleType(ImageButton.ScaleType.FIT_CENTER);
    brightness.setPadding(padding, padding, padding, padding);
    brightness.setContentDescription(AlarmStore.label(this, "clock_brightness", "Clock brightness"));
    brightness.setOnClickListener(view -> {
      if (brightnessControls.getVisibility() == View.VISIBLE) brightnessControls.setVisibility(View.GONE);
      else {
        updateBrightnessControls();
        brightnessControls.setVisibility(View.VISIBLE);
      }
    });
    buttons.addView(brightness, new LinearLayout.LayoutParams(size, size));

    final ImageButton close = new ImageButton(this);
    close.setImageDrawable(new CloseIcon());
    close.setBackground(null);
    close.setScaleType(ImageButton.ScaleType.FIT_CENTER);
    close.setPadding(padding, padding, padding, padding);
    close.setContentDescription(AlarmStore.label(this, "close_clock", "Close clock"));
    close.setOnClickListener(view -> finish());
    buttons.addView(close, new LinearLayout.LayoutParams(size, size));

    brightnessControls = new LinearLayout(this);
    brightnessControls.setOrientation(LinearLayout.VERTICAL);
    brightnessControls.setPadding(ClockView.dp(this, 16), 0, ClockView.dp(this, 16), 0);
    brightnessControls.setVisibility(View.GONE);
    brightnessValue = new TextView(this);
    brightnessValue.setTextColor(ClockView.TIME_COLOR);
    brightnessValue.setGravity(Gravity.CENTER);
    brightnessControls.addView(brightnessValue);
    brightnessSlider = new SeekBar(this);
    brightnessSlider.setMax(99);
    brightnessSlider.setContentDescription(AlarmStore.label(this, "clock_brightness", "Clock brightness"));
    brightnessSlider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
      @Override
      public void onProgressChanged(final SeekBar seekBar, final int progress, final boolean fromUser) {
        if (!fromUser) return;
        final int percent = progress + 1;
        AlarmStore.setClockBrightness(ClockActivity.this, percent);
        setWindowBrightness(ClockFace.windowBrightness(percent));
        brightnessValue.setText(percent + "%");
      }

      @Override
      public void onStartTrackingTouch(final SeekBar seekBar) {}

      @Override
      public void onStopTrackingTouch(final SeekBar seekBar) {}
    });
    brightnessControls.addView(brightnessSlider, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ClockView.dp(this, 48)));
    controls.addView(brightnessControls, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP);
    final int margin = ClockView.dp(this, 12);
    params.setMargins(margin, margin, margin, 0);
    frame.addView(controls, params);
    controls.addOnLayoutChangeListener((view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
        clock.root.setPadding(clock.root.getPaddingLeft(), bottom, clock.root.getPaddingRight(), clock.root.getPaddingBottom()));
    // The clock is laid out into the cutout area; keep the controls clear of the camera cutout.
    frame.setOnApplyWindowInsetsListener((view, insets) -> {
      int top = 0;
      int left = 0;
      int right = 0;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
        final DisplayCutout cutout = insets.getDisplayCutout();
        if (cutout != null) {
          top = cutout.getSafeInsetTop();
          left = cutout.getSafeInsetLeft();
          right = cutout.getSafeInsetRight();
        }
      }
      if (params.topMargin != margin + top || params.leftMargin != margin + left || params.rightMargin != margin + right) {
        params.setMargins(margin + left, margin + top, margin + right, 0);
        controls.setLayoutParams(params);
      }
      return insets;
    });

    setContentView(frame);
    hideSystemBars();

  }

  @Override
  public void onBackPressed() {
    if (brightnessControls.getVisibility() == View.VISIBLE) brightnessControls.setVisibility(View.GONE);
    else super.onBackPressed();
  }

  private void updateBrightnessControls() {

    final int stored = AlarmStore.clockBrightness(this);
    int percent = stored;
    if (stored <= 0) {
      try {
        percent = Math.max(1, Math.round(Settings.System.getInt(getContentResolver(), Settings.System.SCREEN_BRIGHTNESS) * 100.0F / 255.0F));
      }
      catch (Settings.SettingNotFoundException e) {
        Log.w(AlarmStore.TAG, "Could not read the system screen brightness", e);
        percent = 50;
      }
    }
    brightnessSlider.setProgress(Math.max(0, Math.min(99, percent - 1)));
    brightnessValue.setText(stored <= 0 ? AlarmStore.label(this, "system_brightness", "System brightness") : stored + "%");

  }

  @Override
  public void onWindowFocusChanged(final boolean hasFocus) {

    super.onWindowFocusChanged(hasFocus);
    // For example after the notification shade was pulled down.
    if (hasFocus) hideSystemBars();

  }

  @Override
  public void onConfigurationChanged(final Configuration configuration) {

    super.onConfigurationChanged(configuration);
    hideSystemBars();

  }

  // Hides the status and navigation bars; a swipe from the edge shows them briefly, and they hide again by themselves.
  // They belong to this window only, so they return when the clock is closed.
  @SuppressWarnings("deprecation")
  private void hideSystemBars() {

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      getWindow().setDecorFitsSystemWindows(false);
      final WindowInsetsController controller = getWindow().getInsetsController();
      if (controller == null) return;
      controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
      controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
    }
    else {
      getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

  }

  @Override
  protected void onResume() {

    super.onResume();
    setWindowBrightness(ClockFace.windowBrightness(AlarmStore.clockBrightness(this)));
    refresh();

  }

  @Override
  protected void onPause() {

    handler.removeCallbacks(refresh);
    // The clock's brightness applies only while it is shown.
    setWindowBrightness(ClockFace.SYSTEM_BRIGHTNESS);
    super.onPause();

  }

  private void setWindowBrightness(final float brightness) {

    final WindowManager.LayoutParams attributes = getWindow().getAttributes();
    if (attributes.screenBrightness == brightness) return;
    attributes.screenBrightness = brightness;
    getWindow().setAttributes(attributes);

  }

  private void refresh() {

    final long now = System.currentTimeMillis();
    final long next = AlarmStore.nextAlarm(this);
    final long snooze = AlarmStore.nextSnooze(this);
    // A snoozed alarm that goes off first is shown with a minute countdown.
    if (snooze > 0 && snooze <= next) {
      clock.detail.setText(AlarmCountdown.snoozeText(AlarmStore.label(this, "snoozed_countdown", "Snoozed until %1, goes off in %2 min"), ClockView.formatAlarmTime(this, snooze), AlarmCountdown.minutesLeft(now, snooze)));
    }
    else {
      clock.detail.setText(next > 0 ? AlarmStore.label(this, "next_alarm", "Next alarm") + ": " + ClockView.formatAlarmTime(this, next) : AlarmStore.label(this, "no_alarm", "No alarm set"));
    }
    handler.removeCallbacks(refresh);
    // A little after the change, so the countdown has moved on.
    handler.postDelayed(refresh, AlarmCountdown.refreshDelay(now, snooze) + 50);

  }

}
