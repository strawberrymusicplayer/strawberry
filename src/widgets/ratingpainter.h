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

#ifndef RATINGPAINTER_H
#define RATINGPAINTER_H

#include <QPixmap>
#include <QRect>
#include <QPoint>

class QPainter;

class RatingPainter {
 public:
  RatingPainter();

  static constexpr int kStarCount = 5;
  static constexpr int kStarSize = 16;

  static QRect Contents(const QRect rect);
  static float RatingForPos(const QPoint pos, const QRect rect);

  void Paint(QPainter *painter, const QRect rect, float rating) const;

 private:
  QPixmap stars_[kStarCount * 2 + 1];
};

#endif  // RATINGPAINTER_H
