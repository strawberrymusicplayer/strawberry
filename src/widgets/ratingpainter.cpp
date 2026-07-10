/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cmath>

#include <QList>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QRect>
#include <QPoint>

#include "ratingpainter.h"

using namespace Qt::Literals::StringLiterals;

RatingPainter::RatingPainter() {

  const QIcon star_on_icon(u":/pictures/star-on.png"_s);
  const QList<QSize> star_on_icon_sizes = star_on_icon.availableSizes();
  const QPixmap star_on_pixmap = star_on_icon_sizes.isEmpty() ? star_on_icon.pixmap(kStarSize, kStarSize) : star_on_icon.pixmap(star_on_icon_sizes.last());

  const QIcon star_off_icon(u":/pictures/star-off.png"_s);
  const QList<QSize> star_off_icon_sizes = star_off_icon.availableSizes();
  const QPixmap star_off_pixmap = star_off_icon_sizes.isEmpty() ? star_off_icon.pixmap(kStarSize, kStarSize) : star_off_icon.pixmap(star_off_icon_sizes.last());

  // Generate the 10 states, better to do it now than on the fly
  for (int i = 0; i < kStarCount * 2 + 1; ++i) {
    const float rating = static_cast<float>(i) / 2.0F;

    // Clear the pixmap
    stars_[i] = QPixmap(kStarSize * kStarCount, kStarSize);
    stars_[i].fill(Qt::transparent);
    QPainter p(&stars_[i]);

    // Draw the stars
    int x = 0;
    for (int y = 0; y < kStarCount; ++y, x += kStarSize) {
      const QRect rect(x, 0, kStarSize, kStarSize);

      if (rating - 0.25 <= y) {  // Totally empty
        p.drawPixmap(rect, star_off_pixmap);
      }
      else if (rating - 0.75 <= y) {  // Half full
        const QRect target_left(rect.x(), rect.y(), kStarSize / 2, kStarSize);
        const QRect target_right(rect.x() + kStarSize / 2, rect.y(), kStarSize / 2, kStarSize);
        const QRect source_left(0, 0, star_on_pixmap.width() / 2, star_on_pixmap.height());
        const QRect source_right(star_off_pixmap.width() / 2, 0, star_off_pixmap.width() / 2, star_off_pixmap.height());
        p.drawPixmap(target_left, star_on_pixmap, source_left);
        p.drawPixmap(target_right, star_off_pixmap, source_right);
      }
      else {  // Totally full
        p.drawPixmap(rect, star_on_pixmap);
      }
    }
  }

}

QRect RatingPainter::Contents(const QRect rect) {

  const int width = kStarSize * kStarCount;
  const int x = rect.x() + (rect.width() - width) / 2;

  return QRect(x, rect.y(), width, rect.height());

}

float RatingPainter::RatingForPos(const QPoint pos, const QRect rect) {

  const QRect contents = Contents(rect);
  const float raw = static_cast<float>(pos.x() - contents.left()) / static_cast<float>(contents.width());

  // Check if the position was to the right or left of the rectangle.
  if (raw < 0) return 0;
  if (raw > 1) return 1;

  // Round to the nearest 0.1
  return static_cast<float>(lround(raw * kStarCount * 2)) / (kStarCount * 2);

}

void RatingPainter::Paint(QPainter *painter, const QRect rect, float rating) const {

  QSize size(qMin(kStarSize * kStarCount, rect.width()), qMin(kStarSize, rect.height()));
  QPoint pos(rect.center() - QPoint(size.width() / 2, size.height() / 2));

  rating *= kStarCount;

  // Draw the stars
  const int star = qBound(0, static_cast<int>(lround(rating * 2.0)), kStarCount * 2);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->drawPixmap(QRect(pos, size), stars_[star], QRect(QPoint(0, 0), size));

}
