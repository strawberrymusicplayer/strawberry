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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QEnterEvent;
#else
class QEvent;
#endif
class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;

class VolumeSlider : public SliderSlider {
  Q_OBJECT

 public:
  explicit VolumeSlider(QWidget *parent, uint max = 0);
  void SetEnabled(const bool enabled);

 protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEnterEvent*) override;
#else
  void enterEvent(QEvent*) override;
#endif
  void leaveEvent(QEvent*) override;
  void paintEvent(QPaintEvent*) override;
  virtual void paletteChange(const QPalette&);
  void slideEvent(QMouseEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent *e) override;

 private slots:
  virtual void slotAnimTimer();

 private:
  static const int ANIM_INTERVAL = 18;
  static const int ANIM_MAX = 18;

  VolumeSlider(const VolumeSlider&);
  VolumeSlider &operator=(const VolumeSlider&);

  void generateGradient();
  QPixmap drawVolumePixmap() const;
  void drawVolumeSliderHandle();

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
