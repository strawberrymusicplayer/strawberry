/***************************************************************************
                        sliderslider.cpp
                        -------------------
  begin                : Dec 15 2003
  copyright            : (C) 2003 by Mark Kretschmann
  email                : markey@web.de
  copyright            : (C) 2005 by Gábor Lehel
  email                : illissius@gmail.com
  copyright            : (C) 2018-2023 by Jonas Kvinge
  email                : jonas@jkvinge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QMouseEvent>
#include <QWheelEvent>

#include "sliderslider.h"

SliderSlider::SliderSlider(const Qt::Orientation orientation, QWidget *parent, const int max)
    : QSlider(orientation, parent),
      sliding_(false),
      wheeling_(false),
      outside_(false),
      prev_value_(0) {

  setRange(0, max);

}

void SliderSlider::SetValue(const uint new_value) {

  setValue(static_cast<int>(new_value));

}

void SliderSlider::setValue(int new_value) {

  // Don't adjust the slider while the user is dragging it!

  if ((!sliding_ || outside_) && !wheeling_) {
    QSlider::setValue(adjustValue(new_value));
  }
  else {
    prev_value_ = new_value;
  }

}

int SliderSlider::adjustValue(int v) const {
  int mp = (minimum() + maximum()) / 2;
  return orientation() == Qt::Vertical ? mp - (v - mp) : v;
}

void SliderSlider::slideEvent(QMouseEvent *e) {

  QStyleOptionSlider option;
  initStyleOption(&option);
  QRect sliderRect(style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this));

  QSlider::setValue(
      orientation() == Qt::Horizontal
          ? ((QApplication::layoutDirection() == Qt::RightToLeft)
                 ? QStyle::sliderValueFromPosition(
                       minimum(), maximum(),
                       width() - (e->pos().x() - sliderRect.width() / 2),
                       width() + sliderRect.width(), true)
                 : QStyle::sliderValueFromPosition(
                       minimum(), maximum(),
                       e->pos().x() - sliderRect.width() / 2,
                       width() - sliderRect.width()))
          : QStyle::sliderValueFromPosition(
                minimum(), maximum(), e->pos().y() - sliderRect.height() / 2,
                height() - sliderRect.height()));

}

void SliderSlider::mouseMoveEvent(QMouseEvent *e) {

  if (sliding_) {
    // feels better, but using set value of 20 is bad of course
    QRect rect(-20, -20, width() + 40, height() + 40);

    if (orientation() == Qt::Horizontal && !rect.contains(e->pos())) {
      if (!outside_) QSlider::setValue(prev_value_);
      outside_ = true;
    }
    else {
      outside_ = false;
      slideEvent(e);
      Q_EMIT sliderMoved(value());
    }
  }
  else {
    QSlider::mouseMoveEvent(e);
  }

}

void SliderSlider::mousePressEvent(QMouseEvent *e) {

  QStyleOptionSlider option;
  initStyleOption(&option);
  QRect sliderRect(style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this));

  sliding_ = true;
  prev_value_ = QSlider::value();

  if (!sliderRect.contains(e->pos())) mouseMoveEvent(e);

}

void SliderSlider::mouseReleaseEvent(QMouseEvent *e) {

  Q_UNUSED(e)

  if (!outside_ && QSlider::value() != prev_value_) {
    Q_EMIT SliderReleased(value());
  }

  sliding_ = false;
  outside_ = false;

}

void SliderSlider::wheelEvent(QWheelEvent *e) {

  if (orientation() == Qt::Vertical) {
    // Will be handled by the parent widget
    e->ignore();
    return;
  }

  wheeling_ = true;

  // Position Slider (horizontal)
  int step = e->angleDelta().y() * 1500 / 18;
  int nval = qBound(minimum(), QSlider::value() + step, maximum());

  QSlider::setValue(nval);

  Q_EMIT SliderReleased(value());

  wheeling_ = false;

}
