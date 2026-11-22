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
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

// The Tools > Clock screen: a large clock that keeps the screen on while it is open, with the next alarm below it. Back returns to Tools.
public class ClockActivity extends Activity {

  private static final long REFRESH_MSEC = 30000;

  private final Handler handler = new Handler(Looper.getMainLooper());
  private ClockView clock;
  private final Runnable refresh = this::refresh;

  @Override
  protected void onCreate(final Bundle savedInstanceState) {

    super.onCreate(savedInstanceState);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    clock = new ClockView(this);

    final LinearLayout header = new LinearLayout(this);
    header.setOrientation(LinearLayout.HORIZONTAL);
    header.setGravity(Gravity.CENTER_VERTICAL);
    final Button back = new Button(this);
    back.setText("\u2039");
    back.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
    back.setTextColor(ClockView.DETAIL_COLOR);
    back.setBackground(null);
    back.setContentDescription(AlarmStore.label(this, "back", "Back"));
    back.setMinWidth(ClockView.dp(this, 48));
    back.setMinimumWidth(ClockView.dp(this, 48));
    back.setMinHeight(ClockView.dp(this, 48));
    back.setOnClickListener(view -> finish());
    header.addView(back, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    final TextView breadcrumb = new TextView(this);
    breadcrumb.setText(AlarmStore.label(this, "tools", "Tools") + " > " + AlarmStore.label(this, "clock", "Clock"));
    breadcrumb.setTextColor(ClockView.DETAIL_COLOR);
    breadcrumb.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
    breadcrumb.setTypeface(Typeface.DEFAULT_BOLD);
    header.addView(breadcrumb, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0F));
    clock.root.addView(header, 0, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

    setContentView(clock.root);

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

    final long next = AlarmStore.nextAlarm(this);
    clock.detail.setText(next > 0 ? AlarmStore.label(this, "next_alarm", "Next alarm") + ": " + ClockView.formatAlarmTime(this, next) : AlarmStore.label(this, "no_alarm", "No alarm set"));
    handler.removeCallbacks(refresh);
    handler.postDelayed(refresh, REFRESH_MSEC);

  }

}
