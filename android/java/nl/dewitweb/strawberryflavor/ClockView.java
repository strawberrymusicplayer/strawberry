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
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.format.DateFormat;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextClock;
import android.widget.TextView;

import java.util.Date;
import java.util.Locale;

// The clock shared by the Tools > Clock screen and the alarm screen, so both look the same: a large light digital time or a painted analog face, chosen in Clock settings, with the date below it on a black background.
final class ClockView {

  static final int BACKGROUND = Color.BLACK;
  static final int TIME_COLOR = Color.WHITE;
  static final int DETAIL_COLOR = 0xFFB0B0B0;
  static final int ACCENT_COLOR = 0xFFFF9800;

  final LinearLayout root;
  final LinearLayout bottom;
  final TextView detail;

  ClockView(final Activity activity) {

    root = new LinearLayout(activity);
    root.setOrientation(LinearLayout.VERTICAL);
    root.setBackgroundColor(BACKGROUND);
    final int padding = dp(activity, 24);
    root.setPadding(padding, padding, padding, padding);
    root.setFitsSystemWindows(true);

    // The time fills the free space, in portrait and landscape.
    final View time;
    if (AlarmStore.clockType(activity) == ClockFace.ANALOG) {
      time = new AnalogClockView(activity);
    }
    else {
      final TextClock digital = new TextClock(activity);
      digital.setFormat24Hour("HH:mm");
      digital.setFormat12Hour("h:mm");
      digital.setTextColor(TIME_COLOR);
      digital.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
      digital.setGravity(Gravity.CENTER);
      digital.setMaxLines(1);
      digital.setIncludeFontPadding(false);
      digital.setAutoSizeTextTypeUniformWithConfiguration(24, 400, 2, TypedValue.COMPLEX_UNIT_SP);
      time = digital;
    }
    root.addView(time, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 2.0F));

    final TextClock date = new TextClock(activity);
    final String pattern = DateFormat.getBestDateTimePattern(Locale.getDefault(), "EEEEdMMMM");
    date.setFormat24Hour(pattern);
    date.setFormat12Hour(pattern);
    date.setTextColor(DETAIL_COLOR);
    date.setTextSize(TypedValue.COMPLEX_UNIT_SP, 24);
    date.setGravity(Gravity.CENTER);
    root.addView(date, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

    detail = new TextView(activity);
    detail.setTextColor(DETAIL_COLOR);
    detail.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
    detail.setGravity(Gravity.CENTER);
    detail.setPadding(0, dp(activity, 16), 0, 0);
    root.addView(detail, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

    bottom = new LinearLayout(activity);
    bottom.setOrientation(LinearLayout.VERTICAL);
    bottom.setGravity(Gravity.CENTER);
    root.addView(bottom, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0F));

  }

  static int dp(final Activity activity, final int value) {
    return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, activity.getResources().getDisplayMetrics()));
  }

  // Formats a time in milliseconds since the epoch as the weekday and time, following the device's 12 or 24 hour setting.
  static String formatAlarmTime(final Activity activity, final long millis) {

    final String skeleton = DateFormat.is24HourFormat(activity) ? "EEEHHmm" : "EEEhmma";
    return DateFormat.format(DateFormat.getBestDateTimePattern(Locale.getDefault(), skeleton), new Date(millis)).toString();

  }

  // A large rounded button for the bottom area; its text wraps rather than being cut off with large fonts.
  static Button createButton(final Activity activity, final String text, final int color, final int textColor) {

    final Button button = new Button(activity);
    button.setText(text);
    button.setAllCaps(false);
    button.setTextColor(textColor);
    button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
    final GradientDrawable background = new GradientDrawable();
    background.setColor(color);
    background.setCornerRadius(dp(activity, 40));
    button.setBackground(background);
    button.setMinHeight(dp(activity, 96));
    button.setMaxLines(2);
    return button;

  }

  // Shares a row of buttons equally; every button after the first is separated by startMargin.
  static LinearLayout.LayoutParams buttonLayout(final int startMargin) {

    final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0F);
    params.setMarginStart(startMargin);
    return params;

  }

  static void hide(final View view) {
    view.setVisibility(View.GONE);
  }

}
