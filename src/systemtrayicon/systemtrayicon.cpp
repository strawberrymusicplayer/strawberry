/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <cmath>

#include <QPixmap>
#include <QPainter>
#include <QPoint>
#include <QPolygon>
#include <QRect>

#ifdef Q_OS_MACOS
#  include "macsystemtrayicon.h"
#else
#  include "qtsystemtrayicon.h"
#endif

QPixmap SystemTrayIcon::CreateIcon(const QPixmap &pixmap_icon_normal, const QPixmap &pixmap_icon_grey) {

  const qreal dpr = pixmap_icon_normal.devicePixelRatio();
  const QRect pixmap_rect = pixmap_icon_normal.rect();

  QPixmap pixmap_drawn(pixmap_icon_normal.size());
  pixmap_drawn.setDevicePixelRatio(dpr);
  pixmap_drawn.fill(Qt::transparent);

  QPainter p(&pixmap_drawn);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.drawPixmap(0, 0, pixmap_icon_normal);

  if (trayicon_progress_) {
    // The angle of the line that's used to cover the icon.
    // Centered on rect.topLeft()
    const double angle = static_cast<double>(100 - song_progress_) / 100.0 * M_PI_2;
    const double length = std::sqrt(std::pow(pixmap_rect.width(), 2.0) + std::pow(pixmap_rect.height(), 2.0));

    QPolygon mask;
    mask << pixmap_rect.topLeft();
    mask << pixmap_rect.topLeft() + QPoint(static_cast<int>(length * std::sin(angle)), static_cast<int>(length * std::cos(angle)));

    if (song_progress_ > 50) {
      mask << pixmap_rect.bottomRight();
    }

    mask << pixmap_rect.topRight();
    mask << pixmap_rect.topLeft();

    // Draw the grey bit
    p.setClipRegion(mask);
    p.drawPixmap(0, 0, pixmap_icon_grey);
    p.setClipping(false);
  }

  // Draw the playing or paused icon in the top-right
  if (!current_state_icon_.isNull()) {
    const int height = pixmap_rect.height() / 2;
    QPixmap current_state_scaled = current_state_icon_.scaledToHeight(height, Qt::SmoothTransformation);
    current_state_scaled.setDevicePixelRatio(dpr);

    const QRect state_rect(static_cast<int>((pixmap_rect.width() - current_state_scaled.width()) / dpr), 0, static_cast<int>(current_state_scaled.width() / dpr), static_cast<int>(current_state_scaled.height() / dpr));
    p.drawPixmap(state_rect, current_state_scaled);
  }

  p.end();

  return pixmap_drawn;

}
