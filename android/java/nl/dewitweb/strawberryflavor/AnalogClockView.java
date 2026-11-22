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

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.format.DateFormat;
import android.view.View;

import java.util.Calendar;

// A painted round clock face with tick marks and hour, minute and thin second hands, filling the largest square that fits.
final class AnalogClockView extends View {

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private int describedMinute = -1;

  AnalogClockView(final Context context) {

    super(context);
    paint.setStrokeCap(Paint.Cap.ROUND);

  }

  @Override
  protected void onDraw(final Canvas canvas) {

    super.onDraw(canvas);
    final Calendar now = Calendar.getInstance();
    final int hour = now.get(Calendar.HOUR_OF_DAY);
    final int minute = now.get(Calendar.MINUTE);
    final int second = now.get(Calendar.SECOND);

    final float cx = getPaddingLeft() + (getWidth() - getPaddingLeft() - getPaddingRight()) / 2.0F;
    final float cy = getPaddingTop() + (getHeight() - getPaddingTop() - getPaddingBottom()) / 2.0F;
    final float radius = Math.min(getWidth() - getPaddingLeft() - getPaddingRight(), getHeight() - getPaddingTop() - getPaddingBottom()) / 2.0F * 0.96F;
    if (radius <= 0) return;

    paint.setStyle(Paint.Style.STROKE);
    paint.setColor(ClockView.DETAIL_COLOR);
    paint.setStrokeWidth(radius * 0.015F);
    canvas.drawCircle(cx, cy, radius, paint);

    for (int tick = 0; tick < 60; ++tick) {
      final boolean hourTick = tick % 5 == 0;
      paint.setColor(hourTick ? ClockView.TIME_COLOR : ClockView.DETAIL_COLOR);
      paint.setStrokeWidth(radius * (hourTick ? 0.035F : 0.012F));
      drawHand(canvas, cx, cy, tick * 6.0F, radius * (hourTick ? 0.80F : 0.88F), radius * 0.93F);
    }

    paint.setColor(ClockView.TIME_COLOR);
    paint.setStrokeWidth(radius * 0.06F);
    drawHand(canvas, cx, cy, ClockFace.hourAngle(hour, minute, second), 0, radius * 0.52F);
    paint.setStrokeWidth(radius * 0.04F);
    drawHand(canvas, cx, cy, ClockFace.minuteAngle(minute, second), 0, radius * 0.78F);
    paint.setColor(ClockView.ACCENT_COLOR);
    paint.setStrokeWidth(radius * 0.012F);
    drawHand(canvas, cx, cy, ClockFace.secondAngle(second), -radius * 0.12F, radius * 0.86F);
    paint.setStyle(Paint.Style.FILL);
    canvas.drawCircle(cx, cy, radius * 0.035F, paint);

    if (minute != describedMinute) {
      describedMinute = minute;
      setContentDescription(DateFormat.getTimeFormat(getContext()).format(now.getTime()));
    }
    // Redraws at the next second while the view is shown.
    postInvalidateDelayed(1000 - now.get(Calendar.MILLISECOND));

  }

  // Draws a line along the angle in degrees clockwise from 12 o'clock, from one distance from the center to another.
  private void drawHand(final Canvas canvas, final float cx, final float cy, final float angle, final float from, final float to) {

    final double radians = Math.toRadians(angle);
    final float dx = (float) Math.sin(radians);
    final float dy = (float) -Math.cos(radians);
    canvas.drawLine(cx + dx * from, cy + dy * from, cx + dx * to, cy + dy * to, paint);

  }

}
