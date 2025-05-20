/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "ratingwidget.h"

#include <cmath>

#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>

using namespace Qt::Literals::StringLiterals;

RatingPainter::RatingPainter() {

  // Load the base pixmaps
  QIcon star_on(u":/pictures/star-on.png"_s);
  QList<QSize> star_on_sizes = star_on.availableSizes();
  QPixmap on(star_on.pixmap(star_on_sizes.last()));
  QIcon star_off(u":/pictures/star-off.png"_s);
  QList<QSize> star_off_sizes = star_off.availableSizes();
  QPixmap off(star_off.pixmap(star_off_sizes.last()));

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
        p.drawPixmap(rect, off);
      }
      else if (rating - 0.75 <= y) {  // Half full
        const QRect target_left(rect.x(), rect.y(), kStarSize / 2, kStarSize);
        const QRect target_right(rect.x() + kStarSize / 2, rect.y(), kStarSize / 2, kStarSize);
        const QRect source_left(0, 0, kStarSize / 2, kStarSize);
        const QRect source_right(kStarSize / 2, 0, kStarSize / 2, kStarSize);
        p.drawPixmap(target_left, on, source_left);
        p.drawPixmap(target_right, off, source_right);
      }
      else {  // Totally full
        p.drawPixmap(rect, on);
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
  painter->drawPixmap(QRect(pos, size), stars_[star], QRect(QPoint(0, 0), size));

}

RatingWidget::RatingWidget(QWidget *parent) : QWidget(parent), rating_(0.0), hover_rating_(-1.0) {

  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

}

QSize RatingWidget::sizeHint() const {

  const int frame_width = 1 + style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  return QSize(RatingPainter::kStarSize * (RatingPainter::kStarCount + 2) + frame_width * 2, RatingPainter::kStarSize + frame_width * 2);

}

void RatingWidget::set_rating(const float rating) {

  rating_ = rating;
  update();

}

void RatingWidget::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QStylePainter p(this);

  // Draw the background
  QStyleOptionFrame opt;
  opt.initFrom(this);
  opt.state |= QStyle::State_Sunken;
  opt.frameShape = QFrame::StyledPanel;
  opt.lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, this);
  opt.midLineWidth = 0;

  p.drawPrimitive(QStyle::PE_PanelLineEdit, opt);

  // Draw the stars
  painter_.Paint(&p, rect(), hover_rating_ == -1.0 ? rating_ : hover_rating_);

}

void RatingWidget::mousePressEvent(QMouseEvent *e) {

  rating_ = RatingPainter::RatingForPos(e->pos(), rect());
  Q_EMIT RatingChanged(rating_);

}

void RatingWidget::mouseMoveEvent(QMouseEvent *e) {

  hover_rating_ = RatingPainter::RatingForPos(e->pos(), rect());
  update();

}

void RatingWidget::leaveEvent(QEvent *e) {

  Q_UNUSED(e)

  hover_rating_ = -1.0;
  update();

}

void RatingWidget::keyPressEvent(QKeyEvent *e) {

  constexpr float arrow_incr = 0.5f / RatingPainter::kStarCount;

  float rating = -1.0f;

  if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) {
    rating = qBound(0.0f, static_cast<float>(e->key() - Qt::Key_0) / RatingPainter::kStarCount, 1.0f);
  }
  else if (e->key() == Qt::Key_Left) {
    rating = qBound(0.0f, rating_ - arrow_incr, 1.0f);
  }
  else if (e->key() == Qt::Key_Right) {
    rating = qBound(0.0f, rating_ + arrow_incr, 1.0f);
  }

  if (rating != -1.0f) {
    if (rating != rating_) {
      rating_ = rating;
      Q_EMIT RatingChanged(rating_);
    }
  }
  else {
    QWidget::keyPressEvent(e);
  }

}
