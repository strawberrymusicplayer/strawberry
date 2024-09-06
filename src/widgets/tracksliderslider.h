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

#ifndef TRACKSLIDERSLIDER_H
#define TRACKSLIDERSLIDER_H

#include <QObject>
#include <QString>
#include <QSlider>

class QWidget;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QEnterEvent;

#ifndef Q_OS_MACOS
class TrackSliderPopup;
#endif

// It's the slider inside the TrackSliderSlider
class TrackSliderSlider : public QSlider {
  Q_OBJECT

 public:
  explicit TrackSliderSlider(QWidget *parent = nullptr);

 Q_SIGNALS:
  void SeekForward();
  void SeekBackward();
  void Previous();
  void Next();

 protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void enterEvent(QEnterEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void keyPressEvent(QKeyEvent *event) override;

 private Q_SLOTS:
  void UpdateDeltaTime();

 private:
  // Units are eighths of a degree
  static const int WHEEL_ROTATION_TO_SEEK = 120;

#ifndef Q_OS_MACOS
  TrackSliderPopup *popup_;
#endif

  int mouse_hover_seconds_;

  int wheel_accumulator_;
};

#endif  // TRACKSLIDERSLIDER_H
