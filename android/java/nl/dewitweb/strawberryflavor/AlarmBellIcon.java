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

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

final class AlarmBellIcon extends Drawable {

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  AlarmBellIcon() {
    paint.setColor(CloseIcon.COLOR);
    paint.setStyle(Paint.Style.STROKE);
    paint.setStrokeCap(Paint.Cap.ROUND);
    paint.setStrokeJoin(Paint.Join.ROUND);
  }

  @Override
  public void draw(final Canvas canvas) {

    final Rect bounds = getBounds();
    final float size = Math.min(bounds.width(), bounds.height());
    final int save = canvas.save();
    canvas.translate(bounds.exactCenterX() - size / 2.0F, bounds.exactCenterY() - size / 2.0F);
    canvas.scale(size / 64.0F, size / 64.0F);
    paint.setStrokeWidth(4.0F);
    final Path bell = new Path();
    bell.moveTo(12, 46);
    bell.lineTo(17, 40);
    bell.lineTo(17, 27);
    bell.cubicTo(17, 18, 23, 12, 32, 12);
    bell.cubicTo(41, 12, 47, 18, 47, 27);
    bell.lineTo(47, 40);
    bell.lineTo(52, 46);
    bell.close();
    canvas.drawPath(bell, paint);
    canvas.drawLine(32, 7, 32, 12, paint);
    canvas.drawArc(26, 44, 38, 56, 0, 180, false, paint);
    canvas.restoreToCount(save);

  }

  @Override
  public void setAlpha(final int alpha) {
    paint.setAlpha(alpha);
  }

  @Override
  public void setColorFilter(final ColorFilter filter) {
    paint.setColorFilter(filter);
  }

  @Override
  public int getOpacity() {
    return PixelFormat.TRANSLUCENT;
  }

}
