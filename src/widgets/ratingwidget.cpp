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

#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QEvent>

#include "ratingwidget.h"
#include "ratingpainter.h"

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

  if (rating == rating_) return;

  rating_ = rating;
  update();

  Q_EMIT RatingValueChanged(rating_);

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

  const float rating = RatingPainter::RatingForPos(e->pos(), rect());
  if (rating != rating_) {
    rating_ = rating;
    Q_EMIT RatingValueChanged(rating_);
  }
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
      update();
      Q_EMIT RatingValueChanged(rating_);
      Q_EMIT RatingChanged(rating_);
    }
  }
  else {
    QWidget::keyPressEvent(e);
  }

}
