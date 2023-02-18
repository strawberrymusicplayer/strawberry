/***************************************************************************
                        prettyslider.h
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

#ifndef PRETTYSLIDER_H
#define PRETTYSLIDER_H

#include <QObject>

#include "sliderslider.h"

class QMouseEvent;

class PrettySlider : public SliderSlider {
  Q_OBJECT

 public:
  enum class SliderMode {
    Normal,  // Same behavior as Slider *unless* there's a moodbar
    Pretty
  };

  explicit PrettySlider(const Qt::Orientation orientation, const SliderMode mode, QWidget *parent, const uint max = 0);

 protected:
  void slideEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;

 private:
  PrettySlider(const PrettySlider&);             // undefined
  PrettySlider &operator=(const PrettySlider&);  // undefined

  SliderMode m_mode;
};

#endif  // PRETTYSLIDER_H
