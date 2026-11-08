/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2018, Vikram Ambrose <ambroseworks@gmail.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FANCYTABBAR_H
#define FANCYTABBAR_H

#include <QTabBar>
#include <QString>
#include <QPoint>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPointingDevice;

class FancyTabBar : public QTabBar {
  Q_OBJECT

 public:
  explicit FancyTabBar(QWidget *parent = nullptr);

  QString TabText(const int index) const;
  QSize sizeHint() const override;
  int width() const;

  // Dragging scrolls the tabs instead of moving them, and a tab is only selected when it is tapped without dragging.
  void SetTouchScrolling(const bool enabled);
  bool touch_scrolling() const { return touch_scrolling_; }

 protected:
  QSize tabSizeHint(const int index) const override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *pe) override;

 private:
  void ScrollBy(const int delta);

 private:
  int mouseHoverTabIndex;
  bool touch_scrolling_;
  bool touch_pressed_;
  bool touch_dragging_;
  int touch_drag_distance_;
  QPoint touch_press_position_;
  QPoint touch_last_position_;
  QPointingDevice *touch_scroll_device_;
};

#endif  // FANCYTABBAR_H
