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
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

// A painted red X in a thin red circle, for closing the fullscreen clock; muted, so it does not distract at night.
final class CloseIcon extends Drawable {

  static final int COLOR = 0xB0E53935;

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  CloseIcon() {
    paint.setColor(COLOR);
    paint.setStyle(Paint.Style.STROKE);
    paint.setStrokeCap(Paint.Cap.ROUND);
  }

  @Override
  public void draw(final Canvas canvas) {

    final Rect bounds = getBounds();
    final float size = Math.min(bounds.width(), bounds.height());
    final float cx = bounds.exactCenterX();
    final float cy = bounds.exactCenterY();
    paint.setStrokeWidth(size * 0.05F);
    canvas.drawCircle(cx, cy, size * 0.42F, paint);
    paint.setStrokeWidth(size * 0.09F);
    final float arm = size * 0.18F;
    canvas.drawLine(cx - arm, cy - arm, cx + arm, cy + arm, paint);
    canvas.drawLine(cx - arm, cy + arm, cx + arm, cy - arm, paint);

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
