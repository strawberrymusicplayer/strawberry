/***************************************************************************
                        volumeslider.h
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

#ifndef VOLUMESLIDER_H
#define VOLUMESLIDER_H

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QPixmap>
#include <QPalette>
#include <QColor>

#include "sliderslider.h"

class QTimer;
class QEnterEvent;
class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class QEvent;

class VolumeSlider : public SliderSlider {
  Q_OBJECT

 public:
  explicit VolumeSlider(QWidget *parent, uint max = 0);
  void SetEnabled(const bool enabled);
  void HandleWheel(const int delta);

 protected:
  void enterEvent(QEnterEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  virtual void paletteChange(const QPalette &palette);
  void slideEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;
  void mousePressEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent *e) override;

 private Q_SLOTS:
  virtual void slotAnimTimer();

 private:
  static const int ANIM_INTERVAL = 18;
  static const int ANIM_MAX = 18;

  // Units are eighths of a degree
  static const int WHEEL_ROTATION_PER_STEP = 30;

  VolumeSlider(const VolumeSlider&);
  VolumeSlider &operator=(const VolumeSlider&);

  void generateGradient();
  QPixmap drawVolumePixmap() const;
  void drawVolumeSliderHandle();

  int wheel_accumulator_;

  bool anim_enter_;
  int anim_count_;
  QTimer *timer_anim_;

  QPixmap pixmap_inset_;
  QPixmap pixmap_gradient_;

  QColor previous_theme_text_color_;
  QColor previous_theme_highlight_color_;

  QList<QPixmap> handle_pixmaps_;
};

#endif  // VOLUMESLIDER_H
