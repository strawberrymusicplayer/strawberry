/***************************************************************************
                       sliderslider.h
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

#ifndef SLIDERSLIDER_H
#define SLIDERSLIDER_H

#include <QObject>
#include <QSlider>

class QMouseEvent;
class QWheelEvent;

class SliderSlider : public QSlider {
  Q_OBJECT

 public:
  explicit SliderSlider(const Qt::Orientation, QWidget*, const int max = 0);

  // WARNING non-virtual - and thus only really intended for internal use this is a major flaw in the class presently, however it suits our current needs fine
  int value() const { return adjustValue(QSlider::value()); }

  virtual void SetValue(const uint new_value);
  virtual void setValue(int new_value);

 Q_SIGNALS:
  // We emit this when the user has specifically changed the slider so connect to it if valueChanged() is too generic Qt also emits valueChanged(int)
  void SliderReleased(const int);

 protected:
  virtual void slideEvent(QMouseEvent*);
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;

  bool sliding_;
  bool wheeling_;

  /// we flip the value for vertical sliders
  int adjustValue(int v) const;

 private:
  bool outside_;
  int prev_value_;

  SliderSlider(const SliderSlider&);
  SliderSlider &operator=(const SliderSlider&);
};

#endif  // SLIDERSLIDER_H
