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

#ifndef RATINGWIDGET_H
#define RATINGWIDGET_H

#include <QWidget>

class QPaintEvent;
class QMouseEvent;
class QKeyEvent;
class QEvent;

#include "ratingpainter.h"

class RatingWidget : public QWidget {
  Q_OBJECT

  Q_PROPERTY(float rating READ rating WRITE set_rating NOTIFY RatingValueChanged)

 public:
  RatingWidget(QWidget *parent = nullptr);

  QSize sizeHint() const override;

  float rating() const { return rating_; }
  void set_rating(const float rating);

 Q_SIGNALS:
  // Emitted only when the user changes the rating through the widget, not on programmatic set_rating() calls.
  void RatingChanged(const float rating);
  // Property NOTIFY signal, emitted on any rating change including set_rating().
  void RatingValueChanged(const float rating);

 protected:
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;

 private:
  RatingPainter painter_;
  float rating_;
  float hover_rating_;
};

#endif  // RATINGWIDGET_H
