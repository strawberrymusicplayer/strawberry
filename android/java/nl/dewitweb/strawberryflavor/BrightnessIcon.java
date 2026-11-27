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
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

final class BrightnessIcon extends Drawable {

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  BrightnessIcon() {
    paint.setColor(Color.WHITE);
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
    final Path bulb = new Path();
    bulb.moveTo(24, 43);
    bulb.cubicTo(24, 37, 17, 34, 17, 25);
    bulb.cubicTo(17, 16, 23, 10, 32, 10);
    bulb.cubicTo(41, 10, 47, 16, 47, 25);
    bulb.cubicTo(47, 34, 40, 37, 40, 43);
    bulb.close();
    canvas.drawPath(bulb, paint);
    canvas.drawLine(24, 49, 40, 49, paint);
    canvas.drawLine(28, 55, 36, 55, paint);
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
