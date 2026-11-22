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
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.Button;

// The full-screen ringing alarm, also shown over the lock screen, with the clock, the alarm name and big Snooze and Stop buttons.
public class AlarmActivity extends Activity {

  @Override
  protected void onCreate(final Bundle savedInstanceState) {

    super.onCreate(savedInstanceState);
    setShowWhenLocked(true);
    setTurnScreenOn(true);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    final ClockView clock = new ClockView(this);
    final String name = AlarmService.ringingName();
    clock.detail.setText(name.isEmpty() ? AlarmStore.label(this, "alarm", "Alarm") : name);
    clock.detail.setTextColor(ClockView.TIME_COLOR);
    clock.detail.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 32);

    final Button snooze = ClockView.createButton(this, AlarmStore.label(this, "snooze", "Snooze"), 0xFF424242, ClockView.TIME_COLOR);
    snooze.setOnClickListener(view -> send(AlarmService.ACTION_SNOOZE));
    clock.bottom.addView(snooze, ClockView.buttonLayout(this));
    final Button stop = ClockView.createButton(this, AlarmStore.label(this, "stop", "Stop"), ClockView.ACCENT_COLOR, ClockView.BACKGROUND);
    stop.setOnClickListener(view -> send(AlarmService.ACTION_STOP));
    clock.bottom.addView(stop, ClockView.buttonLayout(this));

    setContentView(clock.root);
    AlarmService.setFinishedListener(this::finish);

  }

  @Override
  protected void onResume() {

    super.onResume();
    // The alarm may have been stopped from the notification while this screen was away.
    if (!AlarmService.isRinging()) finish();

  }

  @Override
  protected void onDestroy() {

    AlarmService.setFinishedListener(null);
    super.onDestroy();

  }

  // Back must not silence the alarm by accident; Snooze or Stop does.
  @Override
  @SuppressWarnings("deprecation")
  public void onBackPressed() {}

  private void send(final String action) {

    startService(new Intent(this, AlarmService.class).setAction(action));
    finish();

  }

}
