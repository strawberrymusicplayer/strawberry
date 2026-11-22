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
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageButton;

// The Tools > Clock screen: a fullscreen clock without system bars that keeps the screen on while it is open, with the next alarm below it.
// The red X and Back return to Tools.
public class ClockActivity extends Activity {

  private final Handler handler = new Handler(Looper.getMainLooper());
  private ClockView clock;
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
    // Without buttons the time takes all free space; the top padding keeps it clear of the close button.
    clock.bottom.setVisibility(View.GONE);
    clock.root.setPadding(clock.root.getPaddingLeft(), ClockView.dp(this, 64), clock.root.getPaddingRight(), clock.root.getPaddingBottom());

    final FrameLayout frame = new FrameLayout(this);
    frame.addView(clock.root, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    final ImageButton close = new ImageButton(this);
    close.setImageDrawable(new CloseIcon());
    close.setBackground(null);
    close.setScaleType(ImageButton.ScaleType.FIT_CENTER);
    final int padding = ClockView.dp(this, 8);
    close.setPadding(padding, padding, padding, padding);
    close.setContentDescription(AlarmStore.label(this, "close_clock", "Close clock"));
    close.setOnClickListener(view -> finish());
    final int size = ClockView.dp(this, 56);
    final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(size, size, Gravity.TOP | Gravity.END);
    params.setMargins(0, ClockView.dp(this, 12), ClockView.dp(this, 12), 0);
    frame.addView(close, params);

    setContentView(frame);
    hideSystemBars();

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
    refresh();

  }

  @Override
  protected void onPause() {

    handler.removeCallbacks(refresh);
    super.onPause();

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
