/***************************************************************************
                        prettyslider.cpp
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

#include <QSlider>
#include <QStyle>
#include <QMouseEvent>

#include "prettyslider.h"

PrettySlider::PrettySlider(const Qt::Orientation orientation, const SliderMode mode, QWidget *parent, const uint max)
    : SliderSlider(orientation, parent, static_cast<int>(max)), m_mode(mode) {

  if (m_mode == SliderMode::Pretty) {
    setFocusPolicy(Qt::NoFocus);
  }

}

void PrettySlider::mousePressEvent(QMouseEvent *e) {

  SliderSlider::mousePressEvent(e);

  slideEvent(e);

}

void PrettySlider::slideEvent(QMouseEvent *e) {

  if (m_mode == SliderMode::Pretty) {
    QSlider::setValue(orientation() == Qt::Horizontal ? QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().x(), width() - 2) : QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().y(), height() - 2));  // clazy:exclude=skipped-base-method
  }
  else {
    SliderSlider::slideEvent(e);
  }

}
